#include "testfw.h"

#include "sources.h"

using namespace gwheel;

TEST_CASE(sources_publish_and_read_back_frame)
{
    sources::Frame f;
    f.axes.steer       = -0.25f;
    f.axes.throttle    = 0.5f;
    f.axes.brake       = 0.75f;
    f.digital.buttons  = 0b1011;
    f.digital.pov      = 9000;
    f.connected        = true;
    sources::Publish(f);

    const sources::Frame g = sources::Current();
    CHECK_EQ(g.axes.steer, -0.25f);
    CHECK_EQ(g.axes.throttle, 0.5f);
    CHECK_EQ(g.axes.brake, 0.75f);
    CHECK_EQ(g.digital.buttons, 0b1011u);
    CHECK_EQ(g.digital.pov, static_cast<uint16_t>(9000));
    CHECK(g.connected);

    sources::Publish(sources::Frame{});
    CHECK(!sources::Current().connected);
}

TEST_CASE(sources_in_vehicle_and_radio_flags)
{
    sources::SetInVehicle(true);
    CHECK(sources::InVehicle());
    sources::SetInVehicle(false);
    CHECK(!sources::InVehicle());

    sources::SetRadioActive(true);
    CHECK(sources::RadioActive());
    sources::SetRadioActive(false);
    CHECK(!sources::RadioActive());
}

TEST_CASE(sources_engine_rpm_clamps_negative_to_zero)
{
    sources::SetEngineRpmRaw(3500.f);
    CHECK_EQ(sources::EngineRpmRaw(), 3500.f);
    sources::SetEngineRpmRaw(-20.f);
    CHECK_EQ(sources::EngineRpmRaw(), 0.f);
}

TEST_CASE(sources_shift_rpm_uses_per_gear_value_inside_range)
{
    const float shifts[] = { 6000.f, 6200.f, 6400.f };
    sources::SetEngineProfile(800.f, 7000.f, shifts, 3);
    CHECK_EQ(sources::IdleRpm(), 800.f);
    CHECK_EQ(sources::RedlineRpm(), 7000.f);
    CHECK_EQ(sources::ShiftRpmForGear(1), 6000.f);
    CHECK_EQ(sources::ShiftRpmForGear(3), 6400.f);
}

TEST_CASE(sources_shift_rpm_falls_back_to_redline_outside_range)
{
    const float shifts[] = { 6000.f, 0.f };
    sources::SetEngineProfile(800.f, 7000.f, shifts, 2);
    CHECK_EQ(sources::ShiftRpmForGear(0), 7000.f);   // neutral
    CHECK_EQ(sources::ShiftRpmForGear(-1), 7000.f);  // reverse
    CHECK_EQ(sources::ShiftRpmForGear(3), 7000.f);   // beyond gear count
    CHECK_EQ(sources::ShiftRpmForGear(2), 7000.f);   // unset gear (0)
}

TEST_CASE(sources_engine_profile_sanitises_inputs)
{
    float shifts[sources::kMaxGears + 4];
    for (auto& s : shifts) s = 5000.f;
    shifts[0] = -1.f;
    sources::SetEngineProfile(-100.f, -1.f, shifts, sources::kMaxGears + 4);
    CHECK_EQ(sources::IdleRpm(), 0.f);
    CHECK_EQ(sources::RedlineRpm(), 0.f);
    CHECK_EQ(sources::ShiftRpmForGear(1), 0.f);              // negative shift -> 0 -> redline (0)
    CHECK_EQ(sources::ShiftRpmForGear(sources::kMaxGears), 5000.f);
    CHECK_EQ(sources::ShiftRpmForGear(sources::kMaxGears + 1), 0.f);

    // Null shift array and negative count are tolerated.
    sources::SetEngineProfile(900.f, 6500.f, nullptr, 3);
    CHECK_EQ(sources::ShiftRpmForGear(2), 6500.f);
    sources::SetEngineProfile(900.f, 6500.f, shifts, -5);
    CHECK_EQ(sources::ShiftRpmForGear(1), 6500.f);
}

TEST_CASE(sources_clear_engine_profile_resets_everything)
{
    const float shifts[] = { 6000.f };
    sources::SetEngineProfile(800.f, 7000.f, shifts, 1);
    sources::SetCurrentGear(4);
    sources::ClearEngineProfile();
    CHECK_EQ(sources::IdleRpm(), 0.f);
    CHECK_EQ(sources::RedlineRpm(), 0.f);
    CHECK_EQ(sources::ShiftRpmForGear(1), 0.f);
    CHECK_EQ(sources::CurrentGear(), 0);
}

TEST_CASE(sources_current_gear_round_trips)
{
    sources::SetCurrentGear(-1);
    CHECK_EQ(sources::CurrentGear(), -1);
    sources::SetCurrentGear(5);
    CHECK_EQ(sources::CurrentGear(), 5);
    sources::SetCurrentGear(0);
}
