#include "testfw.h"
#include "support/fake_backend.h"
#include "support/test_hooks.h"

#include "vehicle_hook.cpp"

#include <algorithm>
#include <cstring>
#include <memory>

using namespace gwheel;
using namespace gwheel::vehicle_hook;
using testsupport::FakeBackend;

// The detour is called with a plain zeroed buffer standing in for the
// game's vehicle object. FFB is switched off in config for these tests, so
// the detour stops after the input write instead of reading the vehicle's
// physics through game RTTI (which only exists in the game).

namespace
{
    struct FakeVehicle
    {
        static constexpr size_t kSize =
            std::max<size_t>(sizeof(RED4ext::vehicle::BaseObject), off::kInputSteer + sizeof(float)) + 64;
        std::unique_ptr<std::byte[]> mem{ new std::byte[kSize]() };

        void*  self()      { return mem.get(); }
        float& steer()     { return *FloatFieldAt(mem.get(), off::kInputSteer); }
        float& throttle()  { return static_cast<RED4ext::vehicle::BaseObject*>(self())->acceleration; }
        float& brake()     { return static_cast<RED4ext::vehicle::BaseObject*>(self())->deceleration; }
    };

    struct Fixture
    {
        FakeBackend backend;
        FakeVehicle car;
        Fixture()
        {
            testsupport::InstallBackend(&backend);
            testsupport::ResetConfigToDefaults();
            config::SetFfbEnabled(false);
            config::SetSteerGainPct(200);
            SetPlayerVehicle(car.self());
        }
        ~Fixture()
        {
            SetPlayerVehicle(nullptr);
            sources::Publish(sources::Frame{});
            testsupport::ResetConfigToDefaults();
            testsupport::ResetFfbState();
            testsupport::InstallBackend(nullptr);
        }
    };

    void PublishWheel(float steer, float throttle = 0.f, float brake = 0.f, bool connected = true)
    {
        sources::Frame f;
        f.connected      = connected;
        f.axes.steer     = steer;
        f.axes.throttle  = throttle;
        f.axes.brake     = brake;
        sources::Publish(f);
    }
}

TEST_CASE(vehicle_hook_player_vehicle_tracking)
{
    int a = 0, b = 0;
    SetPlayerVehicle(&a);
    CHECK(IsPlayerVehicle(&a));
    CHECK(!IsPlayerVehicle(&b));
    CHECK(!IsPlayerVehicle(nullptr));
    CHECK_EQ(GetPlayerVehicle(), static_cast<void*>(&a));
    CHECK(sources::InVehicle());
    SetPlayerVehicle(nullptr);
    CHECK(!sources::InVehicle());
    CHECK(GetPlayerVehicle() == nullptr);
    CHECK(!IsInstalled());
}

TEST_CASE(vehicle_hook_writes_scaled_steer_and_pedals)
{
    Fixture fx;
    PublishWheel(0.3f, 0.6f, 0.2f);
    DetourUpdateVehicleCameraInput(fx.car.self());
    CHECK_NEAR(fx.car.steer(), 0.6f, 1e-6);       // 0.3 x 200% sensitivity
    CHECK_NEAR(fx.car.throttle(), 0.6f, 1e-6);
    CHECK_NEAR(fx.car.brake(), 0.2f, 1e-6);
}

TEST_CASE(vehicle_hook_sensitivity_saturates_at_full_lock)
{
    Fixture fx;
    config::SetSteerGainPct(400);
    PublishWheel(-0.5f, 1.5f, -1.f);
    DetourUpdateVehicleCameraInput(fx.car.self());
    CHECK_NEAR(fx.car.steer(), -1.f, 1e-6);
    CHECK_NEAR(fx.car.throttle(), 1.f, 1e-6);
    CHECK_NEAR(fx.car.brake(), 0.f, 1e-6);
}

TEST_CASE(vehicle_hook_keeps_larger_keyboard_input)
{
    Fixture fx;
    fx.car.steer()    = -0.9f;   // what the game's own input already wrote
    fx.car.throttle() = 0.8f;
    PublishWheel(0.2f, 0.5f);
    DetourUpdateVehicleCameraInput(fx.car.self());
    CHECK_NEAR(fx.car.steer(), -0.9f, 1e-6);
    CHECK_NEAR(fx.car.throttle(), 0.8f, 1e-6);
}

TEST_CASE(vehicle_hook_invert_direction_flips_steer)
{
    Fixture fx;
    config::SetFfbInvertDirection(true);
    PublishWheel(0.25f);
    DetourUpdateVehicleCameraInput(fx.car.self());
    CHECK_NEAR(fx.car.steer(), -0.5f, 1e-6);
}

TEST_CASE(vehicle_hook_ignores_other_vehicles_and_disconnected_wheel)
{
    Fixture fx;
    FakeVehicle other;
    PublishWheel(0.5f, 0.5f);
    DetourUpdateVehicleCameraInput(other.self());
    CHECK_EQ(other.steer(), 0.f);

    PublishWheel(0.5f, 0.5f, 0.f, false);
    DetourUpdateVehicleCameraInput(fx.car.self());
    CHECK_EQ(fx.car.steer(), 0.f);

    DetourUpdateVehicleCameraInput(nullptr);       // tolerated
    SetPlayerVehicle(nullptr);
    PublishWheel(0.5f, 0.5f);
    DetourUpdateVehicleCameraInput(fx.car.self());
    CHECK_EQ(fx.car.steer(), 0.f);
}

TEST_CASE(vehicle_hook_disabled_input_writes_nothing)
{
    Fixture fx;
    config::SetInputEnabled(false);
    PublishWheel(0.5f, 0.5f, 0.5f);
    DetourUpdateVehicleCameraInput(fx.car.self());
    CHECK_EQ(fx.car.steer(), 0.f);
    CHECK_EQ(fx.car.throttle(), 0.f);
    CHECK_EQ(fx.car.brake(), 0.f);
}

TEST_CASE(vehicle_hook_ffb_off_releases_effects)
{
    Fixture fx;
    testsupport::ResetFfbState();
    // Get effects playing with one FFB tick for a moving car.
    tweakdb_physics::PerCarPhysics car{};
    car.kind           = tweakdb_physics::ChassisKind::Car;
    car.chainResolved  = true;
    car.liveStateValid = true;
    car.speedMps       = 20.f;
    wheel::FfbDriverState driver;
    driver.steerNormalized = 0.3f;
    wheel::FfbUserScalars scalars;
    scalars.ffbEnabled = true;
    wheel::ApplyFfbFromPhysics(car, driver, scalars);
    REQUIRE(fx.backend.Count("PlayConstant") == 1);
    fx.backend.calls.clear();

    // With FFB off in config, the detour tears them down.
    PublishWheel(0.3f);
    DetourUpdateVehicleCameraInput(fx.car.self());
    CHECK_EQ(fx.backend.Count("StopConstant"), 1);
    CHECK_EQ(fx.backend.Count("StopSpring"), 1);
    CHECK_EQ(fx.backend.Count("PlayConstant"), 0);
}

TEST_CASE(vehicle_hook_counts_detour_calls)
{
    Fixture fx;
    const uint64_t before = FireCount();
    DetourUpdateVehicleCameraInput(fx.car.self());
    DetourUpdateVehicleCameraInput(nullptr);
    CHECK_EQ(FireCount(), before + 2);
}
