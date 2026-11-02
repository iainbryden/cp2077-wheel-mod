#include "testfw.h"
#include "support/test_hooks.h"

#include "axis_resolver.cpp"

#include <vector>

using namespace gwheel;
using namespace gwheel::axis_resolver;

namespace
{
    wheel::Snapshot ConnectedSnap()
    {
        wheel::Snapshot s;
        s.connected = true;
        return s;
    }

    std::vector<std::string> Tokens(const std::string& s)
    {
        std::vector<std::string> out;
        ForEachToken(s, [&](const char* p, size_t n) { out.emplace_back(p, n); });
        return out;
    }
}

TEST_CASE(axis_for_each_token_skips_empty_tokens)
{
    CHECK(Tokens("").empty());
    CHECK(Tokens("|||").empty());
    const auto t = Tokens("|a||bc|");
    REQUIRE(t.size() == 2);
    CHECK_EQ(t[0], std::string("a"));
    CHECK_EQ(t[1], std::string("bc"));
}

TEST_CASE(axis_parse_wheel_axis_token)
{
    std::string name;
    const std::string ok = "wheel-axis:lY";
    CHECK(ParseWheelAxis(ok.data(), ok.size(), name));
    CHECK_EQ(name, std::string("lY"));

    const std::string bare = "wheel-axis:";
    CHECK(!ParseWheelAxis(bare.data(), bare.size(), name));
    const std::string button = "wheel:5";
    CHECK(!ParseWheelAxis(button.data(), button.size(), name));
    const std::string key = "keyboard:KEY_H";
    CHECK(!ParseWheelAxis(key.data(), key.size(), name));
}

TEST_CASE(axis_shape_deadzone_zeroes_small_input_and_rescales)
{
    CHECK_EQ(ShapeSignedSteer(0.01f, 2, "default"), 0.f);
    CHECK_EQ(ShapeSignedSteer(-0.02f, 2, "default"), 0.f);
    // Full lock still reaches 1 after the deadzone is removed.
    CHECK_NEAR(ShapeSignedSteer(1.f, 10, "default"), 1.f, 1e-6);
    CHECK_NEAR(ShapeSignedSteer(-1.f, 10, "default"), -1.f, 1e-6);
    // Halfway between deadzone edge and lock maps to 0.5.
    CHECK_NEAR(ShapeSignedSteer(0.55f, 10, "default"), 0.5f, 1e-5);
    // Zero deadzone is a pass-through.
    CHECK_NEAR(ShapeSignedSteer(0.3f, 0, "default"), 0.3f, 1e-6);
}

TEST_CASE(axis_shape_deadzone_percent_is_clamped)
{
    // 150% would divide by a negative range; the clamp keeps it at 99%.
    CHECK_EQ(ShapeSignedSteer(0.5f, 150, "default"), 0.f);
    CHECK_NEAR(ShapeSignedSteer(1.f, 150, "default"), 1.f, 1e-6);
    // Negative deadzone behaves as zero.
    CHECK_NEAR(ShapeSignedSteer(0.3f, -5, "default"), 0.3f, 1e-6);
}

TEST_CASE(axis_shape_curves_keep_sign_and_endpoints)
{
    for (const char* curve : { "default", "subdued", "sharp" })
    {
        CHECK_NEAR(ShapeSignedSteer(1.f, 0, curve), 1.f, 1e-6);
        CHECK_NEAR(ShapeSignedSteer(-1.f, 0, curve), -1.f, 1e-6);
        CHECK_EQ(ShapeSignedSteer(0.f, 0, curve), 0.f);
        CHECK(ShapeSignedSteer(-0.4f, 0, curve) < 0.f);
    }
    // Subdued is softer than linear near center, sharp is stronger.
    CHECK_NEAR(ShapeSignedSteer(0.5f, 0, "subdued"), std::pow(0.5f, 1.7f), 1e-6);
    CHECK_NEAR(ShapeSignedSteer(0.5f, 0, "sharp"), std::pow(0.5f, 0.6f), 1e-6);
    CHECK(ShapeSignedSteer(0.5f, 0, "subdued") < 0.5f);
    CHECK(ShapeSignedSteer(0.5f, 0, "sharp") > 0.5f);
    // Unknown curve names fall back to linear.
    CHECK_NEAR(ShapeSignedSteer(0.5f, 0, "bogus"), 0.5f, 1e-6);
}

TEST_CASE(axis_shape_clamps_out_of_range_input)
{
    CHECK_NEAR(ShapeSignedSteer(1.7f, 0, "default"), 1.f, 1e-6);
    CHECK_NEAR(ShapeSignedSteer(-3.f, 5, "sharp"), -1.f, 1e-6);
}

TEST_CASE(axis_binding_store_set_get_clear)
{
    Clear();
    SetBinding("SteerAxis", "wheel-axis:lX");
    CHECK_EQ(GetBinding("SteerAxis"), std::string("wheel-axis:lX"));
    SetBinding("SteerAxis", "");
    CHECK(GetBinding("SteerAxis").empty());
    SetBinding("BrakeAxis", "wheel-axis:lRz");
    SetBinding("BrakeAxis", nullptr);
    CHECK(GetBinding("BrakeAxis").empty());
    SetBinding(nullptr, "x");
    CHECK(GetBinding(nullptr).empty());
    SetBinding("ThrottleAxis", "wheel-axis:lY");
    Clear();
    CHECK(GetBinding("ThrottleAxis").empty());
}

TEST_CASE(axis_is_wheel_bound_requires_an_axis_token)
{
    Clear();
    CHECK(!IsActionWheelBound("SteerAxis"));
    SetBinding("SteerAxis", "keyboard:KEY_A|wheel:3");
    CHECK(!IsActionWheelBound("SteerAxis"));
    SetBinding("SteerAxis", "keyboard:KEY_A|wheel-axis:lX");
    CHECK(IsActionWheelBound("SteerAxis"));
    Clear();
}

TEST_CASE(axis_resolve_returns_zero_when_disconnected_or_unbound)
{
    testsupport::ResetConfigToDefaults();
    Clear();
    wheel::Snapshot s = ConnectedSnap();
    s.axes[wheel::Axis_lY] = 0.8f;
    CHECK_EQ(Resolve("ThrottleAxis", s, Combine::UnsignedMax), 0.f);  // unbound

    SetBinding("ThrottleAxis", "wheel-axis:lY");
    s.connected = false;
    CHECK_EQ(Resolve("ThrottleAxis", s, Combine::UnsignedMax), 0.f);  // disconnected

    SetBinding("ThrottleAxis", "wheel-axis:nope|wheel:2");
    s.connected = true;
    CHECK_EQ(Resolve("ThrottleAxis", s, Combine::UnsignedMax), 0.f);  // no valid axis
    Clear();
}

TEST_CASE(axis_resolve_pedals_take_max_and_clamp)
{
    testsupport::ResetConfigToDefaults();
    Clear();
    wheel::Snapshot s = ConnectedSnap();
    s.axes[wheel::Axis_lRz]     = 0.3f;
    s.axes[wheel::Axis_slider0] = 0.9f;
    SetBinding("BrakeAxis", "wheel-axis:lRz|wheel-axis:slider0");
    CHECK_NEAR(Resolve("BrakeAxis", s, Combine::UnsignedMax), 0.9f, 1e-6);

    // A negative phantom reading can't subtract from a real press.
    s.axes[wheel::Axis_slider0] = -0.5f;
    CHECK_NEAR(Resolve("BrakeAxis", s, Combine::UnsignedMax), 0.3f, 1e-6);

    s.axes[wheel::Axis_lRz] = 1.4f;
    CHECK_NEAR(Resolve("BrakeAxis", s, Combine::UnsignedMax), 1.f, 1e-6);
    Clear();
}

TEST_CASE(axis_resolve_steer_uses_first_source_and_shapes_it)
{
    // Every config change republishes the bindings from config, so bind
    // through config here rather than directly.
    testsupport::ResetConfigToDefaults();
    config::SetSteerDeadzonePct(0);
    config::SetResponseCurve("default");
    config::SetActionBinding("SteerAxis", "wheel-axis:lX|wheel-axis:lRx");
    wheel::Snapshot s = ConnectedSnap();
    s.axes[wheel::Axis_lX]  = -0.4f;
    s.axes[wheel::Axis_lRx] = 0.9f;
    // First bound source wins even though the second is larger.
    CHECK_NEAR(Resolve("SteerAxis", s, Combine::SignedFirst), -0.4f, 1e-6);

    // The configured curve is applied to steer.
    config::SetResponseCurve("sharp");
    CHECK_NEAR(Resolve("SteerAxis", s, Combine::SignedFirst), -std::pow(0.4f, 0.6f), 1e-6);

    // And the configured deadzone.
    config::SetResponseCurve("default");
    config::SetSteerDeadzonePct(15);
    s.axes[wheel::Axis_lX] = 0.1f;
    CHECK_EQ(Resolve("SteerAxis", s, Combine::SignedFirst), 0.f);
    Clear();
    testsupport::ResetConfigToDefaults();
}
