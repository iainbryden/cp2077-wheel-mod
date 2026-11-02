#include "testfw.h"
#include "support/fake_backend.h"
#include "support/test_hooks.h"

#include "wheel.cpp"
#include "wheel_tf_pipeline.cpp"

using namespace gwheel;
using namespace gwheel::wheel;
using testsupport::FakeBackend;

namespace testsupport
{
    void InstallBackend(FakeBackend* backend)
    {
        // g_backend owns its pointer; tests own theirs. Release before
        // replacing so a test's backend is never deleted by the plugin.
        (void)g_backend.release();
        g_backend.reset(backend);
    }

    void ResetFfbState()
    {
        using namespace centering_state;
        Reset();
        s_lastCoefSentNs.store(0);
        s_lastConstSentNs.store(0);
        s_lastDamperSentNs.store(0);
        s_lastBumpySentNs.store(0);
        s_lastCallTickMs.store(0);
        s_surfaceBaselineMag.store(0.f);
        wheel_tf::Reset();
    }
}

namespace
{
    // Never let the plugin delete a test-owned backend at exit.
    struct ReleaseBackendAtExit
    {
        ~ReleaseBackendAtExit() { (void)g_backend.release(); }
    } g_releaseBackendAtExit;

    tweakdb_physics::PerCarPhysics Car(float speedMps = 20.f)
    {
        tweakdb_physics::PerCarPhysics c{};
        c.kind                         = tweakdb_physics::ChassisKind::Car;
        c.chainResolved                = true;
        c.liveStateValid               = true;
        c.speedMps                     = speedMps;
        c.turnUpdateBaseSpeedThreshold = 9.f;    // cruise = 18 m/s
        c.chassisMass                  = 1400.f; // baseline 0.75
        c.maxWheelTurnDeg              = 45.f;
        return c;
    }

    FfbDriverState Driver(float steer = 0.3f)
    {
        FfbDriverState d;
        d.steerNormalized = steer;
        return d;
    }

    FfbUserScalars Scalars()
    {
        FfbUserScalars s;
        s.ffbEnabled      = true;
        s.wheelRangeDeg   = 180;   // range scale 1.0
        s.yawFeedbackPct  = 50;
        s.activeTorquePct = 50;
        s.stationaryMps   = 0.5f;
        return s;
    }

    struct Fixture
    {
        FakeBackend backend;
        Fixture()
        {
            testsupport::ResetConfigToDefaults();
            testsupport::InstallBackend(&backend);
            testsupport::ResetFfbState();
            backend.calls.clear();
        }
        ~Fixture()
        {
            testsupport::ResetFfbState();
            testsupport::InstallBackend(nullptr);
        }
    };

    // Runs one FFB tick on a fresh edge state and returns the constant force.
    float ConstantFor(FakeBackend& b, const tweakdb_physics::PerCarPhysics& car,
                      const FfbDriverState& d, const FfbUserScalars& s)
    {
        testsupport::ResetFfbState();
        b.calls.clear();
        ApplyFfbFromPhysics(car, d, s);
        const auto* c = b.Last("PlayConstant");
        return c ? c->a : 0.f;
    }

    int64_t NowNs()
    {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }
}

// ---- Axis names ----------------------------------------------------------

TEST_CASE(wheel_axis_names_round_trip)
{
    for (int i = 0; i < Axis_kCount; ++i)
    {
        const Axis a = static_cast<Axis>(i);
        CHECK_EQ(AxisFromName(AxisName(a)), a);
    }
    CHECK_EQ(AxisFromName("lx"), Axis_kCount);   // case-sensitive
    CHECK_EQ(AxisFromName(""), Axis_kCount);
    CHECK_EQ(AxisFromName(nullptr), Axis_kCount);
    CHECK_EQ(std::string(AxisName(Axis_kCount)), std::string(""));
}

// ---- Backend delegation -----------------------------------------------------

TEST_CASE(wheel_calls_without_backend_are_safe)
{
    testsupport::InstallBackend(nullptr);
    CHECK(!IsReady());
    CHECK(!CurrentSnapshot().connected);
    CHECK_EQ(GetCaps().pid, static_cast<uint16_t>(0));
    PlayConstant(0.5f);
    StopAll();
    PlayLeds(0.5f);
    Pump();
    ApplyFfbFromPhysics(Car(), Driver(), Scalars());
    CHECK(true);
}

TEST_CASE(wheel_delegates_to_backend)
{
    Fixture fx;
    PlayConstant(-0.5f);
    PlaySpring(0.2f);
    PlayDamper(0.3f);
    PlayRoadSurface(0.1f, 180);
    PlayCarAirborne();
    StopCarAirborne();
    SetGlobalStrength(0.4f);
    PlayLeds(0.7f);
    PlayLedsRaw(4000.f, 3000.f, 6000.f);
    ClearLeds();
    StopAll();
    CHECK_NEAR(fx.backend.Last("PlayConstant")->a, -0.5f, 1e-6);
    CHECK_NEAR(fx.backend.Last("PlaySpring")->a, 0.2f, 1e-6);
    CHECK_NEAR(fx.backend.Last("PlayDamper")->a, 0.3f, 1e-6);
    CHECK_NEAR(fx.backend.Last("PlayPeriodic")->b, 180.f, 1e-6);
    CHECK_EQ(fx.backend.Count("SetAirborne"), 2);
    CHECK_NEAR(fx.backend.Last("SetGlobalGain")->a, 0.4f, 1e-6);
    CHECK_NEAR(fx.backend.Last("SetLedLevel")->a, 0.7f, 1e-6);
    CHECK_NEAR(fx.backend.Last("PlayLeds")->c, 6000.f, 1e-6);
    CHECK_EQ(fx.backend.Count("ClearLeds"), 1);
    CHECK_EQ(fx.backend.Count("StopAll"), 1);
}

TEST_CASE(wheel_caps_and_snapshot_come_from_backend)
{
    Fixture fx;
    fx.backend.info.vid = 0x046D;
    fx.backend.info.pid = 0xC24F;
    std::strcpy(fx.backend.info.productName, "G29");
    fx.backend.info.operatingRangeDeg = 900;
    fx.backend.caps.ffbConstant = true;
    fx.backend.snap.connected = true;
    fx.backend.snap.buttons = 0x5;

    const Caps& c = GetCaps();
    CHECK_EQ(c.vid, static_cast<uint16_t>(0x046D));
    CHECK_EQ(c.pid, static_cast<uint16_t>(0xC24F));
    CHECK_EQ(std::string(c.productName), std::string("G29"));
    CHECK(c.hasFFB);
    CHECK_EQ(c.operatingRangeDeg, 900);
    CHECK(IsReady());
    CHECK_EQ(CurrentSnapshot().buttons, 0x5u);

    fx.backend.open = false;
    CHECK(!IsReady());
}

// ---- Resend pacing ------------------------------------------------------------

TEST_CASE(wheel_should_resend_policy)
{
    using centering_state::ShouldResend;
    std::atomic<int64_t> last{ NowNs() };

    CHECK(!ShouldResend(30, 30, last));      // unchanged
    CHECK(ShouldResend(35, 30, last));       // coarse step goes out now
    CHECK(ShouldResend(0, 2, last));         // a return to zero goes out now
    last.store(NowNs());
    CHECK(!ShouldResend(31, 30, last));      // fine step inside 8 ms waits

    last.store(NowNs() - 9'000'000);         // 9 ms ago
    CHECK(ShouldResend(31, 30, last));
    CHECK(NowNs() - last.load() < 8'000'000);  // send time was updated
}

// ---- ApplyFfbFromPhysics ----------------------------------------------------

TEST_CASE(ffb_needs_ready_backend_with_ffb)
{
    Fixture fx;
    fx.backend.ffb = false;
    ApplyFfbFromPhysics(Car(), Driver(), Scalars());
    CHECK(fx.backend.calls.empty());
    fx.backend.ffb = true;
    fx.backend.open = false;
    ApplyFfbFromPhysics(Car(), Driver(), Scalars());
    CHECK(fx.backend.calls.empty());
}

TEST_CASE(ffb_moving_car_plays_restoring_forces)
{
    Fixture fx;
    ApplyFfbFromPhysics(Car(), Driver(0.3f), Scalars());

    const auto* spring = fx.backend.Last("PlaySpring");
    const auto* cf     = fx.backend.Last("PlayConstant");
    const auto* damper = fx.backend.Last("PlayDamper");
    REQUIRE(spring && cf && damper);

    // v = 20, cruise = 18 -> speedSq = 1.2346; baseline 0.75; no yaw.
    const float speedSq = (20.f / 18.f) * (20.f / 18.f);
    CHECK_NEAR(spring->a, 0.75f * speedSq, 1e-4);
    // Steer right is pushed back left: shape(0.3) * load 1.5 * active 0.5.
    const float shape = 0.3f * (1.f - 0.3f * 0.3f * 0.3f * 0.3f);
    CHECK_NEAR(cf->a, -shape * 1.5f * 0.5f, 1e-4);
    CHECK_NEAR(damper->a, 0.4f * speedSq, 1e-4);
    // Flat road, no surface baseline: no road effect.
    CHECK_EQ(fx.backend.Count("PlayPeriodic"), 0);
}

TEST_CASE(ffb_force_direction_follows_steer_sign)
{
    Fixture fx;
    const float right = ConstantFor(fx.backend, Car(), Driver(0.4f), Scalars());
    const float left  = ConstantFor(fx.backend, Car(), Driver(-0.4f), Scalars());
    CHECK(right < 0.f);
    CHECK(left > 0.f);
    CHECK_NEAR(right, -left, 1e-6);
}

TEST_CASE(ffb_sign_flip_and_invert_each_negate_the_force)
{
    Fixture fx;
    auto s = Scalars();
    const float base = ConstantFor(fx.backend, Car(), Driver(), s);
    s.ffbOutputSignFlip = true;
    CHECK_NEAR(ConstantFor(fx.backend, Car(), Driver(), s), -base, 1e-6);
    s.ffbInvertDirection = true;
    CHECK_NEAR(ConstantFor(fx.backend, Car(), Driver(), s), base, 1e-6);
    s.ffbOutputSignFlip = false;
    CHECK_NEAR(ConstantFor(fx.backend, Car(), Driver(), s), -base, 1e-6);
}

TEST_CASE(ffb_reverse_scales_forces_to_40_percent)
{
    Fixture fx;
    auto d = Driver();
    const float fwd = ConstantFor(fx.backend, Car(), d, Scalars());
    d.isReversing = true;
    const float rev = ConstantFor(fx.backend, Car(), d, Scalars());
    CHECK_NEAR(rev, fwd * 0.4f, 1e-5);
}

TEST_CASE(ffb_range_scale_uses_user_setting_then_backend)
{
    Fixture fx;
    auto s = Scalars();
    const float at180 = ConstantFor(fx.backend, Car(), Driver(), s);

    s.wheelRangeDeg = 360;                       // half the force
    CHECK_NEAR(ConstantFor(fx.backend, Car(), Driver(), s), at180 * 0.5f, 1e-5);

    s.wheelRangeDeg = 900;                       // 0.2, clamped to 0.3
    CHECK_NEAR(ConstantFor(fx.backend, Car(), Driver(), s), at180 * 0.3f, 1e-5);

    s.wheelRangeDeg = 0;                         // auto: backend reports 540
    fx.backend.rangeDeg = 540;
    CHECK_NEAR(ConstantFor(fx.backend, Car(), Driver(), s), at180 / 3.f, 1e-5);

    // 180/40 = 4.5, clamped to 1.5. Small steer keeps the force unclamped.
    auto d = Driver(0.1f);
    s.wheelRangeDeg = 180;
    const float small180 = ConstantFor(fx.backend, Car(), d, s);
    s.wheelRangeDeg = 40;
    CHECK_NEAR(ConstantFor(fx.backend, Car(), d, s), small180 * 1.5f, 1e-5);
}

TEST_CASE(ffb_active_torque_percent_scales_constant_force)
{
    Fixture fx;
    auto s = Scalars();
    s.activeTorquePct = 100;
    const float full = ConstantFor(fx.backend, Car(), Driver(0.2f), s);
    s.activeTorquePct = 25;
    CHECK_NEAR(ConstantFor(fx.backend, Car(), Driver(0.2f), s), full * 0.25f, 1e-5);
    s.activeTorquePct = 0;
    CHECK_NEAR(ConstantFor(fx.backend, Car(), Driver(0.2f), s), 0.f, 1e-6);
}

TEST_CASE(ffb_constant_force_is_clamped)
{
    Fixture fx;
    auto s = Scalars();
    s.activeTorquePct = 100;
    s.wheelRangeDeg = 40;        // 1.5x
    auto d = Driver(0.67f);      // near the peak of the shape curve
    d.brake = 1.f;               // up to 1.3x weight transfer
    const float f = ConstantFor(fx.backend, Car(60.f), d, s);
    CHECK(f >= -1.f && f <= 1.f);
    CHECK_NEAR(f, -1.f, 1e-6);
}

TEST_CASE(ffb_stationary_car_has_no_forces)
{
    Fixture fx;
    ApplyFfbFromPhysics(Car(0.2f), Driver(0.5f), Scalars());
    CHECK_EQ(fx.backend.Count("PlaySpring"), 0);
    CHECK_EQ(fx.backend.Count("PlayConstant"), 0);
    CHECK_EQ(fx.backend.Count("PlayDamper"), 0);
}

TEST_CASE(ffb_stopping_tears_down_only_what_was_playing)
{
    Fixture fx;
    ApplyFfbFromPhysics(Car(), Driver(), Scalars());
    fx.backend.calls.clear();
    ApplyFfbFromPhysics(Car(0.f), Driver(), Scalars());
    CHECK_EQ(fx.backend.Count("StopSpring"), 1);
    CHECK_EQ(fx.backend.Count("StopConstant"), 1);
    CHECK_EQ(fx.backend.Count("StopDamper"), 1);
    // The road effect never played (0%) but was tracked as sent, so it gets
    // a redundant stop. Harmless; documents current behaviour.
    CHECK_EQ(fx.backend.Count("StopPeriodic"), 1);
    CHECK_EQ(fx.backend.Count("PlayPeriodic"), 0);

    fx.backend.calls.clear();
    ApplyFfbFromPhysics(Car(0.f), Driver(), Scalars());
    CHECK(fx.backend.calls.empty());                  // nothing left to stop
}

TEST_CASE(ffb_disabled_or_unresolved_car_is_treated_as_stopped)
{
    Fixture fx;
    auto s = Scalars();
    s.ffbEnabled = false;
    ApplyFfbFromPhysics(Car(), Driver(), s);
    CHECK_EQ(fx.backend.Count("PlayConstant"), 0);

    auto car = Car();
    car.kind = tweakdb_physics::ChassisKind::Unknown;
    ApplyFfbFromPhysics(car, Driver(), Scalars());
    car = Car(); car.chainResolved = false;
    ApplyFfbFromPhysics(car, Driver(), Scalars());
    car = Car(); car.liveStateValid = false;
    ApplyFfbFromPhysics(car, Driver(), Scalars());
    CHECK_EQ(fx.backend.Count("PlayConstant"), 0);
    CHECK_EQ(fx.backend.Count("PlaySpring"), 0);
}

TEST_CASE(ffb_airborne_plays_effect_once_and_clears_on_landing)
{
    Fixture fx;
    ApplyFfbFromPhysics(Car(), Driver(), Scalars());   // contact forces playing
    fx.backend.calls.clear();

    auto air = Driver();
    air.isOnGround = false;
    ApplyFfbFromPhysics(Car(), air, Scalars());
    CHECK_EQ(fx.backend.Count("SetAirborne"), 1);
    CHECK_NEAR(fx.backend.Last("SetAirborne")->a, 1.f, 1e-6);
    CHECK_EQ(fx.backend.Count("StopConstant"), 1);
    CHECK_EQ(fx.backend.Count("StopSpring"), 1);

    ApplyFfbFromPhysics(Car(), air, Scalars());
    CHECK_EQ(fx.backend.Count("SetAirborne"), 1);      // not retriggered

    ApplyFfbFromPhysics(Car(), Driver(), Scalars());
    CHECK_EQ(fx.backend.Count("SetAirborne"), 2);
    CHECK_NEAR(fx.backend.Last("SetAirborne")->a, 0.f, 1e-6);
    CHECK(fx.backend.Count("PlayConstant") >= 1);
}

TEST_CASE(ffb_unchanged_inputs_are_not_resent)
{
    Fixture fx;
    ApplyFfbFromPhysics(Car(), Driver(), Scalars());
    fx.backend.calls.clear();
    ApplyFfbFromPhysics(Car(), Driver(), Scalars());
    CHECK_EQ(fx.backend.Count("PlaySpring"), 0);
    CHECK_EQ(fx.backend.Count("PlayConstant"), 0);
    CHECK_EQ(fx.backend.Count("PlayDamper"), 0);
}

TEST_CASE(ffb_without_spring_stops_spring_and_keeps_constant)
{
    Fixture fx;
    ApplyFfbFromPhysics(Car(), Driver(), Scalars());
    REQUIRE(fx.backend.Count("PlaySpring") == 1);
    fx.backend.calls.clear();

    auto s = Scalars();
    s.ffbUseSpring = false;
    ApplyFfbFromPhysics(Car(), Driver(0.5f), s);
    CHECK_EQ(fx.backend.Count("StopSpring"), 1);
    CHECK_EQ(fx.backend.Count("PlaySpring"), 0);
    CHECK_EQ(fx.backend.Count("PlayConstant"), 1);
}

TEST_CASE(ffb_sliding_lightens_forces)
{
    Fixture fx;
    auto car = Car();
    const float grip = ConstantFor(fx.backend, car, Driver(), Scalars());
    // Yaw well past the car's limit (yawRef = 45/30 = 1.5 rad/s).
    car.yawMagRadPerSec = 3.f;
    const float slide = ConstantFor(fx.backend, car, Driver(), Scalars());
    CHECK(std::fabs(slide) < std::fabs(grip));
    const auto* spring = fx.backend.Last("PlaySpring");
    REQUIRE(spring);
    // Spring is multiplied by the grip factor exp(-2 * (2 - 1)).
    const float speedSq = (20.f / 18.f) * (20.f / 18.f);
    CHECK_NEAR(spring->a, std::min(1.f, (0.75f * speedSq + 0.5f) * std::exp(-2.f)), 1e-4);
}

TEST_CASE(ffb_bike_lean_softens_spring)
{
    Fixture fx;
    auto bike = Car();
    bike.kind          = tweakdb_physics::ChassisKind::Bike;
    bike.bikeMaxTilt   = 0.7f;
    bike.bikeLiveValid = true;
    bike.bikeCurrentTilt = 0.f;
    ConstantFor(fx.backend, bike, Driver(), Scalars());
    const float upright = fx.backend.Last("PlaySpring")->a;

    bike.bikeCurrentTilt = -0.7f;                 // full lean either way
    ConstantFor(fx.backend, bike, Driver(), Scalars());
    const float leaned = fx.backend.Last("PlaySpring")->a;
    CHECK_NEAR(leaned, upright * 0.15f, 1e-4);
}

TEST_CASE(ffb_jolt_overlays_and_rejects_tiny_kicks)
{
    Fixture fx;
    const float base = ConstantFor(fx.backend, Car(), Driver(0.2f), Scalars());

    testsupport::ResetFfbState();
    TriggerJolt(0.01f, 500);                      // too small to feel: ignored
    TriggerJolt(0.5f, 0);                         // no duration: ignored
    fx.backend.calls.clear();
    ApplyFfbFromPhysics(Car(), Driver(0.2f), Scalars());
    CHECK_NEAR(fx.backend.Last("PlayConstant")->a, base, 1e-6);

    testsupport::ResetFfbState();
    TriggerJolt(0.5f, 10000);
    fx.backend.calls.clear();
    ApplyFfbFromPhysics(Car(), Driver(0.2f), Scalars());
    // Start of a 10 s linear decay: within 1% of the full kick.
    CHECK_NEAR(fx.backend.Last("PlayConstant")->a, base + 0.5f, 0.01);

    TriggerJolt(5.f, 10000);                      // kick is clamped to 1
    CHECK_NEAR(centering_state::s_joltForce.load(), 1.f, 1e-6);
}

TEST_CASE(ffb_surface_baseline_drives_road_effect)
{
    Fixture fx;
    SetSurfaceBaselineMag(2.f);                   // clamped to 0.5
    CHECK_NEAR(s_surfaceBaselineMag.load(), 0.5f, 1e-6);
    SetSurfaceBaselineMag(-1.f);
    CHECK_NEAR(s_surfaceBaselineMag.load(), 0.f, 1e-6);

    SetSurfaceBaselineMag(0.3f);
    ApplyFfbFromPhysics(Car(), Driver(), Scalars());
    const auto* road = fx.backend.Last("PlayPeriodic");
    REQUIRE(road);
    CHECK_NEAR(road->a, 0.3f, 1e-6);
    CHECK_NEAR(road->b, 180.f, 1e-6);
    SetSurfaceBaselineMag(0.f);
}

TEST_CASE(ffb_suspension_activity_ramps_road_effect_slowly)
{
    Fixture fx;
    auto d = Driver();
    d.suspensionActivity = 1.f;                   // target 0.5 after gate/gain
    ApplyFfbFromPhysics(Car(), d, Scalars());
    const auto* road = fx.backend.Last("PlayPeriodic");
    REQUIRE(road);
    CHECK_NEAR(road->a, 0.5f * 0.08f, 1e-5);      // one attack step
    // Activity under the 0.10 gate never starts the effect.
    testsupport::ResetFfbState();
    fx.backend.calls.clear();
    d.suspensionActivity = 0.09f;
    ApplyFfbFromPhysics(Car(), d, Scalars());
    CHECK_EQ(fx.backend.Count("PlayPeriodic"), 0);
}

// ---- Pause watchdog -----------------------------------------------------------

TEST_CASE(wheel_pump_releases_effects_when_gameplay_stalls)
{
    Fixture fx;
    Pump();                                       // no heartbeat yet: nothing
    CHECK_EQ(fx.backend.Count("StopAll"), 0);

    centering_state::s_lastCallTickMs.store(GetTickCount64() - 1000);
    Pump();
    CHECK_EQ(fx.backend.Count("StopAll"), 1);
    Pump();
    CHECK_EQ(fx.backend.Count("StopAll"), 1);     // only once per stall

    centering_state::s_lastCallTickMs.store(GetTickCount64());
    Pump();                                       // re-arms
    centering_state::s_lastCallTickMs.store(GetTickCount64() - 1000);
    Pump();
    CHECK_EQ(fx.backend.Count("StopAll"), 2);
    CHECK_EQ(fx.backend.Count("Pump"), 5);
}

// ---- TrueForce signal pipeline -----------------------------------------------

namespace
{
    void ForceTfConfigRefresh()
    {
        wheel_tf::g_lastConfigCheckTickMs.store(0);
    }

    wheel_tf::Inputs TfInputs()
    {
        wheel_tf::Inputs in;
        in.rpmNorm         = 0.8f;
        in.throttle        = 1.f;
        in.surfaceEnvelope = 0.5f;
        in.skidNorm        = 0.9f;
        return in;
    }
}

TEST_CASE(tf_feed_needs_vibrotactile_backend_and_setting)
{
    Fixture fx;
    config::SetTrueForceFeedSignals(false);
    fx.backend.caps.vibrotactile = true;
    ForceTfConfigRefresh();
    wheel_tf::Feed(TfInputs());
    CHECK(fx.backend.tfSamples.empty());

    config::SetTrueForceFeedSignals(true);
    fx.backend.caps.vibrotactile = false;
    ForceTfConfigRefresh();
    wheel_tf::Feed(TfInputs());
    CHECK(fx.backend.tfSamples.empty());

    testsupport::InstallBackend(nullptr);
    ForceTfConfigRefresh();
    wheel_tf::Feed(TfInputs());                   // no backend: no crash
    testsupport::InstallBackend(&fx.backend);
    testsupport::ResetConfigToDefaults();
}

TEST_CASE(tf_feed_samples_are_capped)
{
    Fixture fx;
    config::SetTrueForceFeedSignals(true);
    fx.backend.caps.vibrotactile = true;
    ForceTfConfigRefresh();
    wheel_tf::Reset();
    for (int i = 0; i < 200; ++i)
    {
        auto in = TfInputs();
        in.surfaceEnvelope = (i % 2) ? 1.f : 0.f; // repeated impacts
        wheel_tf::Feed(in);
    }
    REQUIRE(fx.backend.tfSamples.size() == 200);
    bool anyNonZero = false;
    for (float s : fx.backend.tfSamples)
    {
        CHECK(s >= -0.5f && s <= 0.5f);
        if (s != 0.f) anyNonZero = true;
    }
    CHECK(anyNonZero);
    CHECK(wheel_tf::g_impactBurst.load() <= 0.4f);
    CHECK(wheel_tf::g_enginePhase.load() >= 0.f);
    CHECK(wheel_tf::g_enginePhase.load() < 6.2831853f);
    testsupport::ResetConfigToDefaults();
}

TEST_CASE(tf_feed_is_silent_when_airborne)
{
    Fixture fx;
    config::SetTrueForceFeedSignals(true);
    fx.backend.caps.vibrotactile = true;
    ForceTfConfigRefresh();
    auto in = TfInputs();
    in.isOnGround = false;
    wheel_tf::Feed(in);
    REQUIRE(fx.backend.tfSamples.size() == 1);
    CHECK_EQ(fx.backend.tfSamples[0], 0.f);
    testsupport::ResetConfigToDefaults();
}

TEST_CASE(tf_reset_clears_oscillator_and_burst)
{
    wheel_tf::g_enginePhase.store(1.f);
    wheel_tf::g_impactBurst.store(0.3f);
    wheel_tf::g_lastFeedNs.store(123);
    wheel_tf::Reset();
    CHECK_EQ(wheel_tf::g_enginePhase.load(), 0.f);
    CHECK_EQ(wheel_tf::g_impactBurst.load(), 0.f);
    CHECK_EQ(wheel_tf::g_lastFeedNs.load(), static_cast<uint64_t>(0));
}

TEST_CASE(tf_noise_stays_in_unit_range)
{
    for (int i = 0; i < 10000; ++i)
    {
        const float n = wheel_tf::NoiseSample();
        CHECK(n >= -1.f && n <= 1.f);
    }
}
