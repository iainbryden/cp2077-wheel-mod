#include "testfw.h"
#include "support/fake_logitech_sdk.h"

#include "wheel_backend_logi_legacy.cpp"

#include <cmath>

using namespace gwheel;
using namespace gwheel::wheel_backend;

namespace
{
    // A legacy backend driven by the fake SDK. Bound = SDK initialised and
    // a wheel found in slot 0.
    std::unique_ptr<IWheelBackend> Bound()
    {
        auto b = CreateLogitechLegacyBackend();
        b->Init();
        b->Pump();
        return b;
    }

    float LastPct(const char* fn)
    {
        const auto* c = fakelogi::S().Last(fn);
        return c ? c->a : NAN;
    }
}

TEST_CASE(legacy_identifies_itself)
{
    fakelogi::Reset();
    auto b = CreateLogitechLegacyBackend();
    CHECK(b->GetVendor() == Vendor::Logitech);
    CHECK(b->GetKind() == BackendKind::LogitechLegacy);
    CHECK(b->Init());
    CHECK(b->Open());
    CHECK(!b->IsOpen());                      // opens lazily on Pump
    CHECK_EQ(b->GetHapticRateHz(), 0);
    CHECK(std::isnan(b->GetWheelAngleDeg()));
    CHECK(std::isnan(b->GetWheelAngularVelocityDeg()));
}

TEST_CASE(legacy_stays_closed_while_sdk_init_fails)
{
    fakelogi::Reset();
    fakelogi::S().initResult = false;
    auto b = CreateLogitechLegacyBackend();
    for (int i = 0; i < 3; ++i) b->Pump();
    CHECK(!b->IsOpen());
    CHECK(fakelogi::S().Count("LogiSteeringInitialize") + fakelogi::S().Count("LogiSteeringInitializeWithWindow") == 3);
    b->PlayConstant(1.f);
    CHECK_EQ(fakelogi::S().Count("LogiPlayConstantForce"), 0);
}

TEST_CASE(legacy_waits_for_a_wheel)
{
    fakelogi::Reset();
    fakelogi::S().connectedSlot = -1;
    auto b = CreateLogitechLegacyBackend();
    b->Pump();
    CHECK(!b->IsOpen());
    fakelogi::S().connectedSlot = 0;
    b->Pump();
    CHECK(b->IsOpen());
}

TEST_CASE(legacy_binds_and_reports_device)
{
    fakelogi::Reset();
    auto b = Bound();
    REQUIRE(b->IsOpen());
    CHECK(b->HasFFB());
    CHECK_EQ(std::string(b->GetDeviceInfo().productName), std::string("G29 Driving Force Racing Wheel"));
    CHECK(b->GetDeviceInfo().vendor == Vendor::Logitech);
    CHECK_EQ(b->GetDeviceInfo().sdkMajor, 8);
    const auto& caps = b->GetCapabilities();
    CHECK(caps.ffbConstant && caps.ffbSpring && caps.ffbDamper);
    CHECK(!caps.vibrotactile);
    // G HUB didn't answer the range query: the 540 fallback stays.
    CHECK_EQ(b->GetOperatingRangeDeg(), 540);
    CHECK_EQ(b->GetDeviceInfo().operatingRangeDeg, 0);
}

TEST_CASE(legacy_uses_reported_range_when_available)
{
    fakelogi::Reset();
    fakelogi::S().rangeAnswers = true;
    fakelogi::S().rangeDeg     = 900;
    auto b = Bound();
    CHECK_EQ(b->GetOperatingRangeDeg(), 900);

    // Re-read about once a second (every 250 pumps).
    fakelogi::S().rangeDeg = 270;
    for (int i = 0; i < 248; ++i) b->Pump();
    CHECK_EQ(b->GetOperatingRangeDeg(), 900);
    b->Pump();
    CHECK_EQ(b->GetOperatingRangeDeg(), 270);
}

TEST_CASE(legacy_wheel_without_ffb_plays_nothing)
{
    fakelogi::Reset();
    fakelogi::S().hasFfb = false;
    auto b = Bound();
    REQUIRE(b->IsOpen());
    CHECK(!b->HasFFB());
    b->PlayConstant(0.5f);
    b->PlaySpring(0.5f);
    b->PlayDamper(0.5f);
    b->PlayPeriodic(0.5f, 100);
    b->SetAirborne(true);
    CHECK_EQ(fakelogi::S().Count("LogiPlayConstantForce"), 0);
    CHECK_EQ(fakelogi::S().Count("LogiPlaySpringForce"), 0);
    CHECK_EQ(fakelogi::S().Count("LogiPlayDamperForce"), 0);
    CHECK_EQ(fakelogi::S().Count("LogiPlaySurfaceEffect"), 0);
    CHECK_EQ(fakelogi::S().Count("LogiPlayCarAirborne"), 0);
}

TEST_CASE(legacy_snapshot_from_sdk_state)
{
    fakelogi::Reset();
    fakelogi::S().joy.lX = -32767;
    fakelogi::S().joy.lY = -32768;
    fakelogi::S().joy.rgbButtons[4] = 0x80;
    fakelogi::S().joy.rgdwPOV[0] = 9000;
    auto b = Bound();
    const auto& s = b->CurrentSnapshot();
    CHECK(s.connected);
    CHECK_NEAR(s.axes[wheel::Axis_lX], -1.f, 1e-6);
    CHECK_NEAR(s.axes[wheel::Axis_lY], 1.f, 1e-6);
    CHECK_EQ(s.buttons, 1u << 4);
    CHECK_EQ(s.pov, static_cast<uint16_t>(9000));
}

TEST_CASE(legacy_disconnect_and_rebind)
{
    fakelogi::Reset();
    auto b = Bound();
    REQUIRE(b->IsOpen());
    fakelogi::S().connectedSlot = -1;
    b->Pump();
    CHECK(!b->IsOpen());
    fakelogi::S().connectedSlot = 0;
    b->Pump();
    CHECK(b->IsOpen());
}

TEST_CASE(legacy_effects_scale_to_sdk_percent)
{
    fakelogi::Reset();
    auto b = Bound();
    b->PlayConstant(-0.5f);
    CHECK_EQ(LastPct("LogiPlayConstantForce"), -50.f);
    b->PlayConstant(3.f);
    CHECK_EQ(LastPct("LogiPlayConstantForce"), 100.f);
    b->PlayDamper(0.256f);
    CHECK_EQ(LastPct("LogiPlayDamperForce"), 26.f);
    b->PlayDamper(-1.f);
    CHECK_EQ(LastPct("LogiPlayDamperForce"), 0.f);

    b->PlaySpring(0.3f);
    const auto* spring = fakelogi::S().Last("LogiPlaySpringForce");
    REQUIRE(spring);
    CHECK_EQ(spring->a, 0.f);      // centered
    CHECK_EQ(spring->b, 100.f);    // full saturation
    CHECK_EQ(spring->c, 30.f);

    b->PlayPeriodic(0.2f, 180);
    const auto* road = fakelogi::S().Last("LogiPlaySurfaceEffect");
    REQUIRE(road);
    CHECK_EQ(road->a, static_cast<float>(LOGI_PERIODICTYPE_SINE));
    CHECK_EQ(road->b, 20.f);
    CHECK_EQ(road->c, 180.f);
}

TEST_CASE(legacy_global_gain_multiplies_every_effect)
{
    fakelogi::Reset();
    auto b = Bound();
    b->SetGlobalGain(0.4f);
    b->PlayConstant(-1.f);
    CHECK_EQ(LastPct("LogiPlayConstantForce"), -40.f);
    b->PlaySpring(0.5f);
    CHECK_EQ(fakelogi::S().Last("LogiPlaySpringForce")->c, 20.f);
    b->SetGlobalGain(5.f);          // clamped to 1
    b->PlayConstant(0.7f);
    CHECK_EQ(LastPct("LogiPlayConstantForce"), 70.f);
    b->SetGlobalGain(-1.f);         // clamped to 0
    b->PlayConstant(0.7f);
    CHECK_EQ(LastPct("LogiPlayConstantForce"), 0.f);
}

TEST_CASE(legacy_airborne_and_stop_all)
{
    fakelogi::Reset();
    auto b = Bound();
    b->SetAirborne(true);
    b->SetAirborne(false);
    CHECK_EQ(fakelogi::S().Count("LogiPlayCarAirborne"), 1);
    CHECK_EQ(fakelogi::S().Count("LogiStopCarAirborne"), 1);
    b->StopAll();
    CHECK_EQ(fakelogi::S().Count("LogiStopConstantForce"), 1);
    CHECK_EQ(fakelogi::S().Count("LogiStopDamperForce"), 1);
    CHECK_EQ(fakelogi::S().Count("LogiStopSpringForce"), 1);
    CHECK_EQ(fakelogi::S().Count("LogiStopSurfaceEffect"), 1);
    CHECK_EQ(fakelogi::S().Count("LogiStopCarAirborne"), 2);
}

TEST_CASE(legacy_led_dark_uses_idle_marker_workaround)
{
    fakelogi::Reset();
    auto b = Bound();
    b->SetLedLevel(0.f);
    const auto* c = fakelogi::S().Last("LogiPlayLeds");
    REQUIRE(c);
    // current < firstLed so the G923 idle marker goes dark too.
    CHECK_EQ(c->a, 0.f);
    CHECK_EQ(c->b, 100.f);
    CHECK_EQ(c->c, 200.f);

    b->SetLedLevel(0.5f);
    c = fakelogi::S().Last("LogiPlayLeds");
    CHECK_EQ(c->a, 0.5f);
    CHECK_EQ(c->b, 0.f);
    CHECK_EQ(c->c, 1.f);

    b->SetLedLevel(2.f);
    CHECK_EQ(fakelogi::S().Last("LogiPlayLeds")->a, 1.f);
    b->ClearLeds();
    CHECK_EQ(fakelogi::S().Last("LogiPlayLeds")->c, 200.f);
}

TEST_CASE(legacy_rpm_leds_pass_through_or_go_dark)
{
    fakelogi::Reset();
    auto b = Bound();
    b->PlayLeds(5000.f, 4000.f, 6000.f);
    const auto* c = fakelogi::S().Last("LogiPlayLeds");
    REQUIRE(c);
    CHECK_EQ(c->a, 5000.f);
    CHECK_EQ(c->b, 4000.f);
    CHECK_EQ(c->c, 6000.f);

    b->PlayLeds(3000.f, 4000.f, 6000.f);          // below the first LED
    CHECK_EQ(fakelogi::S().Last("LogiPlayLeds")->c, 200.f);
    b->PlayLeds(5000.f, 6000.f, 6000.f);          // no profile yet
    CHECK_EQ(fakelogi::S().Last("LogiPlayLeds")->c, 200.f);
    b->PlayLeds(0.f, 0.f, 0.f);
    CHECK_EQ(fakelogi::S().Last("LogiPlayLeds")->c, 200.f);
}

TEST_CASE(legacy_leds_ignored_before_bind)
{
    fakelogi::Reset();
    auto b = CreateLogitechLegacyBackend();
    b->SetLedLevel(1.f);
    b->ClearLeds();
    b->PlayLeds(5000.f, 4000.f, 6000.f);
    CHECK_EQ(fakelogi::S().Count("LogiPlayLeds"), 0);
}

TEST_CASE(legacy_operating_range_set_is_clamped)
{
    fakelogi::Reset();
    auto b = Bound();
    b->SetOperatingRangeDeg(10);
    CHECK_EQ(LastPct("LogiSetOperatingRange"), 40.f);
    b->SetOperatingRangeDeg(5000);
    CHECK_EQ(LastPct("LogiSetOperatingRange"), 1080.f);
    b->SetOperatingRangeDeg(900);
    CHECK_EQ(LastPct("LogiSetOperatingRange"), 900.f);
}

TEST_CASE(legacy_close_stops_forces_without_sdk_shutdown)
{
    fakelogi::Reset();
    auto b = Bound();
    b->Close();
    CHECK(!b->IsOpen());
    CHECK_EQ(fakelogi::S().Count("LogiStopSpringForce"), 1);
    CHECK_EQ(fakelogi::S().Count("LogiStopConstantForce"), 1);
    CHECK_EQ(fakelogi::S().Count("LogiStopCarAirborne"), 1);
    CHECK_EQ(fakelogi::S().Last("LogiPlayLeds")->c, 200.f);
    // The shipped SDK DLL crashes in its own teardown; it must not be called.
    CHECK_EQ(fakelogi::S().Count("LogiSteeringShutdown"), 0);
    b->Close();                                   // second close is a no-op
    CHECK_EQ(fakelogi::S().Count("LogiStopSpringForce"), 1);
}
