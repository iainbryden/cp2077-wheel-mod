#include "testfw.h"
#include "support/fake_backend.h"
#include "support/test_hooks.h"

#include "rtti.cpp"

using namespace gwheel;
using namespace gwheel::rtti;
using testsupport::FakeBackend;

// The redscript natives themselves need the game's script VM; the
// capability checks behind the Mod Settings visibility flags are tested.

namespace
{
    struct Fixture
    {
        FakeBackend backend;
        explicit Fixture(uint16_t pid)
        {
            backend.info.pid = pid;
            backend.caps.ffbConstant = false;
            testsupport::InstallBackend(&backend);
        }
        ~Fixture() { testsupport::InstallBackend(nullptr); }
    };
}

TEST_CASE(rtti_capabilities_false_without_a_wheel)
{
    testsupport::InstallBackend(nullptr);
    CHECK(!DetectedHasRightClusterImpl());
    CHECK(!DetectedHasFfbHardwareImpl());
    CHECK(!DetectedHasRevLedsImpl());

    Fixture fx(0xC24F);
    fx.backend.open = false;                 // wheel not bound yet
    CHECK(!DetectedHasRightClusterImpl());
    CHECK(!DetectedHasRevLedsImpl());
}

TEST_CASE(rtti_capabilities_follow_device_table)
{
    {
        Fixture fx(0xC24F);                  // G29
        CHECK(DetectedHasRightClusterImpl());
        CHECK(DetectedHasRevLedsImpl());
        CHECK(DetectedHasFfbHardwareImpl());  // table default
    }
    {
        Fixture fx(0xC262);                  // G920
        CHECK(!DetectedHasRightClusterImpl());
        CHECK(!DetectedHasRevLedsImpl());
    }
    {
        Fixture fx(0xC20E);                  // WingMan Formula GP: no FFB
        CHECK(!DetectedHasFfbHardwareImpl());
        fx.backend.caps.ffbConstant = true;  // the live SDK report wins
        CHECK(DetectedHasFfbHardwareImpl());
    }
}

TEST_CASE(rtti_unknown_wheels_get_permissive_capabilities)
{
    Fixture fx(0xBEEF);
    CHECK(DetectedHasRightClusterImpl());
    CHECK(DetectedHasRevLedsImpl());
    CHECK(DetectedHasFfbHardwareImpl());
}

TEST_CASE(rtti_surface_baseline_is_off_for_every_material)
{
    for (uint64_t h : { 0ull, 1ull, 0xDEADBEEFull, ~0ull })
        CHECK_EQ(SurfaceBaselineForMaterial(h), 0.f);
}
