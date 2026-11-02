#include "testfw.h"

#include "steering_speed.cpp"

#include <fstream>

using namespace gwheel;

// Only the safeguards that run before TweakDB is touched are tested here.
// The scan / validate / rewrite passes need a live game TweakDB (its flat
// storage and record refresh are game functions), so they are covered by
// in-game testing, not by this suite.

namespace
{
    void ResetSteeringSpeedState()
    {
        auto& s = steering_speed::S();
        std::lock_guard lk(s.mtx);
        s.appliedPct      = 100;
        s.sentinelChecked = false;
        s.disabled        = false;
        s.originals.clear();
        steering_speed::ClearSentinel();
    }
}

TEST_CASE(steering_speed_sentinel_lives_next_to_the_module)
{
    const auto p = steering_speed::SentinelPath();
    CHECK_EQ(p.filename().wstring(), std::wstring(L"steering_speed.applying"));
    CHECK(std::filesystem::exists(p.parent_path()));
}

TEST_CASE(steering_speed_sentinel_write_and_clear)
{
    ResetSteeringSpeedState();
    steering_speed::WriteSentinel(300);
    const auto p = steering_speed::SentinelPath();
    REQUIRE(std::filesystem::exists(p));
    std::ifstream in(p);
    std::string line;
    std::getline(in, line);
    in.close();
    CHECK(line.find("300%") != std::string::npos);
    steering_speed::ClearSentinel();
    CHECK(!std::filesystem::exists(p));
    steering_speed::ClearSentinel();           // clearing twice is fine
}

TEST_CASE(steering_speed_100_percent_never_touches_tweakdb)
{
    ResetSteeringSpeedState();
    // Nothing changed this session: no TweakDB access, no marker file.
    CHECK(steering_speed::Apply(100));
    CHECK(!std::filesystem::exists(steering_speed::SentinelPath()));
}

TEST_CASE(steering_speed_skips_session_after_a_crash_mid_write)
{
    ResetSteeringSpeedState();
    steering_speed::WriteSentinel(300);        // left behind by a "crashed" session

    CHECK(!steering_speed::Apply(300));        // skipped, without touching TweakDB
    CHECK(!std::filesystem::exists(steering_speed::SentinelPath()));  // re-armed for next launch
    CHECK(steering_speed::S().disabled);

    CHECK(!steering_speed::Apply(500));        // stays off for the session
    CHECK(!steering_speed::Apply(100));
    ResetSteeringSpeedState();
}

TEST_CASE(steering_speed_limits_are_consistent)
{
    using namespace steering_speed;
    static_assert(kMinPlausibleRate > 0.f);
    static_assert(kMaxWrittenRate <= kMaxPlausibleRate);
    static_assert(kMaxRejectedShare > 0.f && kMaxRejectedShare < 1.f);
    static_assert(kMinFlatsLoaded > 0u);
    static_assert(std::size(kRateFlats) == 2);
    static_assert(std::size(kRecordClasses) == 2);
    CHECK(true);
}
