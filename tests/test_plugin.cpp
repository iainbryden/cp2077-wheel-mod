#include "testfw.h"
#include "support/test_hooks.h"

#include "plugin.cpp"

#include <chrono>

using namespace gwheel;

// OnLoad / OnUnload start the game-facing threads and hooks and need the
// RED4ext SDK, so they are not called. The pump's frame building and its
// timer are tested.

TEST_CASE(plugin_context_starts_empty)
{
    CHECK(Ctx().sdk == nullptr);
    CHECK(&Ctx() == &Ctx());
}

TEST_CASE(plugin_build_frame_routes_bound_axes)
{
    testsupport::ResetConfigToDefaults();
    config::SetSteerDeadzonePct(0);
    config::SetActionBinding("SteerAxis", "wheel-axis:lX");
    config::SetActionBinding("ThrottleAxis", "wheel-axis:lY");
    config::SetActionBinding("BrakeAxis", "wheel-axis:lRz");

    wheel::Snapshot s;
    s.connected = true;
    s.axes[wheel::Axis_lX]  = -0.5f;
    s.axes[wheel::Axis_lY]  = 0.7f;
    s.axes[wheel::Axis_lRz] = 0.2f;
    s.buttons = 0x11;
    s.pov     = 18000;

    const sources::Frame f = BuildFrame(s);
    CHECK(f.connected);
    CHECK_NEAR(f.axes.steer, -0.5f, 1e-6);
    CHECK_NEAR(f.axes.throttle, 0.7f, 1e-6);
    CHECK_NEAR(f.axes.brake, 0.2f, 1e-6);
    CHECK_EQ(f.digital.buttons, 0x11u);
    CHECK_EQ(f.digital.pov, static_cast<uint16_t>(18000));

    s.connected = false;
    const sources::Frame g = BuildFrame(s);
    CHECK(!g.connected);
    CHECK_EQ(g.axes.steer, 0.f);
    CHECK_EQ(g.axes.throttle, 0.f);
    testsupport::ResetConfigToDefaults();
}

TEST_CASE(plugin_build_frame_unbound_axes_are_zero)
{
    testsupport::ResetConfigToDefaults();
    wheel::Snapshot s;
    s.connected = true;
    s.axes[wheel::Axis_lX] = 1.f;
    const sources::Frame f = BuildFrame(s);
    CHECK_EQ(f.axes.steer, 0.f);
    CHECK_EQ(f.axes.throttle, 0.f);
    CHECK_EQ(f.axes.brake, 0.f);
}

TEST_CASE(plugin_pump_timer_sleeps_about_the_period)
{
    using clock = std::chrono::steady_clock;
    PumpTimer timer;
    // Average over many ticks; a loaded machine can stretch single sleeps.
    constexpr int kTicks = 50;
    const auto start = clock::now();
    for (int i = 0; i < kTicks; ++i) timer.SleepFor(kPumpPeriod);
    const double avgMs = std::chrono::duration<double, std::milli>(clock::now() - start).count() / kTicks;
    CHECK(avgMs >= 3.5);
    // Plain sleep_for rounds to the 15.6 ms system tick; the
    // high-resolution timer must do clearly better.
    CHECK(avgMs < 10.0);
}

TEST_CASE(plugin_pump_timer_handles_zero_and_negative)
{
    using clock = std::chrono::steady_clock;
    PumpTimer timer;
    const auto start = clock::now();
    timer.SleepFor(clock::duration::zero());
    timer.SleepFor(-std::chrono::milliseconds(5));
    CHECK(clock::now() - start < std::chrono::milliseconds(50));
}
