#include "testfw.h"
#include "support/fake_backend.h"
#include "support/test_hooks.h"

#include "config.cpp"

#include <fstream>
#include <sstream>
#include <thread>

using namespace gwheel;
using namespace gwheel::config;

namespace testsupport
{
    std::filesystem::path ConfigPathForTests()
    {
        return ResolvePath();
    }

    void ResetConfigToDefaults()
    {
        if (S().path.empty()) S().path = ResolvePath();
        Mutate([](Config& c) { c = Config{}; });
    }
}

namespace
{
    // The plugin writes config.json next to the module that contains it,
    // which in tests is gwheel_tests.exe. Start clean and leave nothing
    // behind.
    struct ConfigFileCleanup
    {
        ConfigFileCleanup()  { Remove(); }
        ~ConfigFileCleanup() { Remove(); }
        static void Remove()
        {
            std::error_code ec;
            std::filesystem::remove(testsupport::ConfigPathForTests(), ec);
        }
    } g_configFileCleanup;

    std::string ReadFile(const std::filesystem::path& p)
    {
        std::ifstream in(p, std::ios::binary);
        std::stringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    void WriteFile(const std::filesystem::path& p, const std::string& text)
    {
        std::ofstream out(p, std::ios::binary | std::ios::trunc);
        out << text;
    }

    Config NonDefaultConfig()
    {
        Config c;
        c.version                       = 7;
        c.input.enabled                 = false;
        c.input.steerDeadzonePct        = 9;
        c.input.responseCurve           = "sharp";
        c.input.steerGainPct            = 175;
        c.input.steerSpeedPct           = 450;
        c.ffb.enabled                   = false;
        c.ffb.debugLogging              = true;
        c.ffb.torquePct                 = 42;
        c.ffb.wheelRangeDeg             = 270;
        c.ffb.stationaryThresholdMps    = 1.25f;
        c.ffb.yawFeedbackPct            = 33;
        c.ffb.activeTorqueStrengthPct   = 66;
        c.ffb.invertDirection           = true;
        c.ffb.outputSignFlip            = true;
        c.ffb.useSpring                 = false;
        c.handshake.playOnStart         = false;
        c.led.enabled                   = false;
        c.led.visualizerWhileMusic      = false;
        c.music.processName             = "Spotify.exe";
        c.trueForce.enabled             = true;
        c.trueForce.feedSignals         = true;
        c.car        = { 1.5f, 11 };
        c.motorcycle = { 0.5f, 12 };
        c.truck      = { 2.0f, 13 };
        c.van        = { 0.25f, 14 };
        c.actionBindings["SteerAxis"] = "wheel-axis:lX";
        c.actionBindings["Horn"]      = "wheel:0|keyboard:KEY_H";
        c.actionBindings["OpenMap"]   = "wheel-pov:0";
        return c;
    }
}

// ---- JSON helpers -----------------------------------------------------------

TEST_CASE(config_escape_json_escapes_control_and_quote_chars)
{
    std::string out;
    EscapeJsonTo(out, "a\"b\\c\n\r\t\b\f");
    CHECK_EQ(out, std::string("\"a\\\"b\\\\c\\n\\r\\t\\b\\f\""));
    out.clear();
    EscapeJsonTo(out, "");
    CHECK_EQ(out, std::string("\"\""));
}

TEST_CASE(config_emit_parse_round_trips_every_field)
{
    const Config in = NonDefaultConfig();
    Config out;
    Parse(Emit(in), out);

    CHECK_EQ(out.version, in.version);
    CHECK_EQ(out.input.enabled, in.input.enabled);
    CHECK_EQ(out.input.steerDeadzonePct, in.input.steerDeadzonePct);
    CHECK_EQ(out.input.responseCurve, in.input.responseCurve);
    CHECK_EQ(out.input.steerGainPct, in.input.steerGainPct);
    CHECK_EQ(out.input.steerSpeedPct, in.input.steerSpeedPct);
    CHECK_EQ(out.ffb.enabled, in.ffb.enabled);
    CHECK_EQ(out.ffb.debugLogging, in.ffb.debugLogging);
    CHECK_EQ(out.ffb.torquePct, in.ffb.torquePct);
    CHECK_EQ(out.ffb.wheelRangeDeg, in.ffb.wheelRangeDeg);
    CHECK_NEAR(out.ffb.stationaryThresholdMps, in.ffb.stationaryThresholdMps, 1e-6);
    CHECK_EQ(out.ffb.yawFeedbackPct, in.ffb.yawFeedbackPct);
    CHECK_EQ(out.ffb.activeTorqueStrengthPct, in.ffb.activeTorqueStrengthPct);
    CHECK_EQ(out.ffb.invertDirection, in.ffb.invertDirection);
    CHECK_EQ(out.ffb.outputSignFlip, in.ffb.outputSignFlip);
    CHECK_EQ(out.ffb.useSpring, in.ffb.useSpring);
    CHECK_EQ(out.handshake.playOnStart, in.handshake.playOnStart);
    CHECK_EQ(out.led.enabled, in.led.enabled);
    CHECK_EQ(out.led.visualizerWhileMusic, in.led.visualizerWhileMusic);
    CHECK_EQ(out.music.processName, in.music.processName);
    CHECK_EQ(out.trueForce.enabled, in.trueForce.enabled);
    CHECK_EQ(out.trueForce.feedSignals, in.trueForce.feedSignals);
    CHECK_NEAR(out.car.steeringMultiplier, 1.5f, 1e-6);
    CHECK_EQ(out.car.responseDelayMs, 11);
    CHECK_NEAR(out.motorcycle.steeringMultiplier, 0.5f, 1e-6);
    CHECK_EQ(out.motorcycle.responseDelayMs, 12);
    CHECK_NEAR(out.truck.steeringMultiplier, 2.0f, 1e-6);
    CHECK_EQ(out.truck.responseDelayMs, 13);
    CHECK_NEAR(out.van.steeringMultiplier, 0.25f, 1e-6);
    CHECK_EQ(out.van.responseDelayMs, 14);
    CHECK(out.actionBindings == in.actionBindings);
}

TEST_CASE(config_emit_sorts_action_bindings)
{
    Config c;
    c.actionBindings["Zeta"]  = "wheel:1";
    c.actionBindings["Alpha"] = "wheel:2";
    const std::string json = Emit(c);
    CHECK(json.find("\"Alpha\"") < json.find("\"Zeta\""));
    Config empty;
    CHECK(Emit(empty).find("\"actionBindings\": {}") != std::string::npos);
}

TEST_CASE(config_parse_missing_fields_keep_defaults)
{
    Config c;
    Parse("{ \"input\": { \"steerGainPct\": 120 } }", c);
    const Config d;
    CHECK_EQ(c.input.steerGainPct, 120);
    CHECK_EQ(c.input.steerSpeedPct, d.input.steerSpeedPct);
    CHECK_EQ(c.input.responseCurve, d.input.responseCurve);
    CHECK_EQ(c.ffb.torquePct, d.ffb.torquePct);
    CHECK_EQ(c.ffb.wheelRangeDeg, d.ffb.wheelRangeDeg);
    CHECK(c.actionBindings.empty());
}

TEST_CASE(config_parse_ignores_malformed_values)
{
    Config c;
    Parse("{ \"input\": { \"enabled\": maybe, \"steerGainPct\": \"x\", \"responseCurve\": sharp } }", c);
    const Config d;
    CHECK_EQ(c.input.enabled, d.input.enabled);
    CHECK_EQ(c.input.steerGainPct, d.input.steerGainPct);
    CHECK_EQ(c.input.responseCurve, d.input.responseCurve);
}

TEST_CASE(config_parse_garbage_does_not_throw)
{
    Config c;
    Parse("", c);
    Parse("not json at all", c);
    Parse("{\"actionBindings\": {\"Horn\": ", c);
    Parse("{\"actionBindings\": {\"Horn\" \"wheel:1\"}", c);
    CHECK(true);
}

TEST_CASE(config_parse_reads_legacy_hello_section)
{
    Config c;
    Parse("{ \"hello\": { \"playOnStart\": false } }", c);
    CHECK(!c.handshake.playOnStart);

    // The current key wins when both are present.
    Config d;
    Parse("{ \"hello\": { \"playOnStart\": false }, \"handshake\": { \"playOnStart\": true } }", d);
    CHECK(d.handshake.playOnStart);
}

TEST_CASE(config_parse_action_bindings_object)
{
    Config c;
    c.actionBindings["Stale"] = "wheel:9";
    ParseActionBindings("{ \"actionBindings\": { \"Horn\": \"wheel:0\", \"Handbrake\" : \"wheel:5\" } }", c);
    CHECK_EQ(c.actionBindings.size(), static_cast<size_t>(2));
    CHECK_EQ(c.actionBindings["Horn"], std::string("wheel:0"));
    CHECK_EQ(c.actionBindings["Handbrake"], std::string("wheel:5"));
    CHECK(c.actionBindings.find("Stale") == c.actionBindings.end());

    ParseActionBindings("{}", c);
    CHECK(c.actionBindings.empty());
}

TEST_CASE(config_parse_keeps_out_of_range_numbers_unclamped)
{
    // Documents current behaviour: Parse copies numbers as written. Range
    // limits are applied by the setters (what Mod Settings calls on every
    // settings push) and by the consumers, not at load time.
    Config c;
    Parse("{ \"input\": { \"steerGainPct\": 5000, \"steerSpeedPct\": -3 }, \"ffb\": { \"torquePct\": 250 } }", c);
    CHECK_EQ(c.input.steerGainPct, 5000);
    CHECK_EQ(c.input.steerSpeedPct, -3);
    CHECK_EQ(c.ffb.torquePct, 250);
}

// ---- Setters ------------------------------------------------------------

TEST_CASE(config_setters_clamp_integer_ranges)
{
    testsupport::ResetConfigToDefaults();

    SetSteerDeadzonePct(-1);   CHECK_EQ(Current().input.steerDeadzonePct, 0);
    SetSteerDeadzonePct(99);   CHECK_EQ(Current().input.steerDeadzonePct, 15);
    SetSteerGainPct(10);       CHECK_EQ(Current().input.steerGainPct, 50);
    SetSteerGainPct(9999);     CHECK_EQ(Current().input.steerGainPct, 400);
    SetSteerGainPct(300);      CHECK_EQ(Current().input.steerGainPct, 300);
    SetSteerSpeedPct(0);       CHECK_EQ(Current().input.steerSpeedPct, 100);
    SetSteerSpeedPct(5000);    CHECK_EQ(Current().input.steerSpeedPct, 1000);
    SetFfbTorquePct(-20);      CHECK_EQ(Current().ffb.torquePct, 0);
    SetFfbTorquePct(140);      CHECK_EQ(Current().ffb.torquePct, 100);
    SetYawFeedbackPct(101);    CHECK_EQ(Current().ffb.yawFeedbackPct, 100);
    SetYawFeedbackPct(-1);     CHECK_EQ(Current().ffb.yawFeedbackPct, 0);
    SetActiveTorqueStrengthPct(500); CHECK_EQ(Current().ffb.activeTorqueStrengthPct, 100);
    SetStationaryThresholdMps(-1.f); CHECK_EQ(Current().ffb.stationaryThresholdMps, 0.f);
    SetStationaryThresholdMps(50.f); CHECK_EQ(Current().ffb.stationaryThresholdMps, 10.f);

    testsupport::ResetConfigToDefaults();
}

TEST_CASE(config_wheel_range_zero_means_auto)
{
    testsupport::ResetConfigToDefaults();
    SetFfbWheelRangeDeg(0);    CHECK_EQ(Current().ffb.wheelRangeDeg, 0);
    SetFfbWheelRangeDeg(-90);  CHECK_EQ(Current().ffb.wheelRangeDeg, 0);
    SetFfbWheelRangeDeg(10);   CHECK_EQ(Current().ffb.wheelRangeDeg, 40);
    SetFfbWheelRangeDeg(5000); CHECK_EQ(Current().ffb.wheelRangeDeg, 1080);
    SetFfbWheelRangeDeg(180);  CHECK_EQ(Current().ffb.wheelRangeDeg, 180);
    testsupport::ResetConfigToDefaults();
}

TEST_CASE(config_bool_and_string_setters)
{
    testsupport::ResetConfigToDefaults();
    SetInputEnabled(false);            CHECK(!Current().input.enabled);
    SetFfbEnabled(false);              CHECK(!Current().ffb.enabled);
    SetFfbInvertDirection(true);       CHECK(Current().ffb.invertDirection);
    SetFfbOutputSignFlip(true);        CHECK(Current().ffb.outputSignFlip);
    SetFfbUseSpring(false);            CHECK(!Current().ffb.useSpring);
    SetHandshakePlayOnStart(false);    CHECK(!Current().handshake.playOnStart);
    SetLedEnabled(false);              CHECK(!Current().led.enabled);
    SetLedVisualizerWhileMusic(false); CHECK(!Current().led.visualizerWhileMusic);
    SetTrueForceEnabled(true);         CHECK(Current().trueForce.enabled);
    SetTrueForceFeedSignals(true);     CHECK(Current().trueForce.feedSignals);
    SetMusicProcessName("firefox.exe");
    CHECK_EQ(Current().music.processName, std::string("firefox.exe"));
    testsupport::ResetConfigToDefaults();
}

TEST_CASE(config_response_curve_rejects_unknown_names)
{
    testsupport::ResetConfigToDefaults();
    SetResponseCurve("subdued");
    CHECK_EQ(Current().input.responseCurve, std::string("subdued"));
    SetResponseCurve("SHARP");
    CHECK_EQ(Current().input.responseCurve, std::string("subdued"));
    SetResponseCurve("");
    CHECK_EQ(Current().input.responseCurve, std::string("subdued"));
    SetResponseCurve("sharp");
    CHECK_EQ(Current().input.responseCurve, std::string("sharp"));
    testsupport::ResetConfigToDefaults();
}

TEST_CASE(config_debug_logging_setter_drives_log_gate)
{
    testsupport::ResetConfigToDefaults();
    SetFfbDebugLogging(true);
    CHECK(log::DebugEnabled());
    SetFfbDebugLogging(false);
    CHECK(!log::DebugEnabled());
}

TEST_CASE(config_action_binding_set_erase_and_fan_out)
{
    testsupport::ResetConfigToDefaults();
    SetActionBinding("SteerAxis", "wheel-axis:lX");
    CHECK_EQ(Current().actionBindings.at("SteerAxis"), std::string("wheel-axis:lX"));
    CHECK_EQ(axis_resolver::GetBinding("SteerAxis"), std::string("wheel-axis:lX"));

    SetActionBinding("SteerAxis", "");
    CHECK(Current().actionBindings.count("SteerAxis") == 0);
    CHECK(axis_resolver::GetBinding("SteerAxis").empty());

    SetActionBinding("", "wheel:1");
    CHECK(Current().actionBindings.empty());
    testsupport::ResetConfigToDefaults();
}

TEST_CASE(config_torque_setter_pushes_global_gain)
{
    testsupport::FakeBackend fake;
    testsupport::InstallBackend(&fake);
    testsupport::ResetConfigToDefaults();
    fake.calls.clear();

    SetFfbTorquePct(40);
    const auto* call = fake.Last("SetGlobalGain");
    REQUIRE(call != nullptr);
    CHECK_NEAR(call->a, 0.4f, 1e-6);

    testsupport::ResetConfigToDefaults();
    testsupport::InstallBackend(nullptr);
}

// ---- Persistence ------------------------------------------------------------

TEST_CASE(config_setter_persists_to_disk)
{
    testsupport::ResetConfigToDefaults();
    SetSteerGainPct(222);
    const std::string text = ReadFile(testsupport::ConfigPathForTests());
    CHECK(text.find("\"steerGainPct\": 222") != std::string::npos);
    CHECK_EQ(ReadAsJson(), Emit(Current()));
    testsupport::ResetConfigToDefaults();
}

TEST_CASE(config_load_reads_file_and_falls_back_to_defaults)
{
    const auto path = testsupport::ConfigPathForTests();
    WriteFile(path, Emit(NonDefaultConfig()));
    Load();
    CHECK_EQ(Current().input.steerGainPct, 175);
    CHECK_EQ(Current().ffb.wheelRangeDeg, 270);
    CHECK_EQ(Current().actionBindings.at("Horn"), std::string("wheel:0|keyboard:KEY_H"));
    // Loading also republishes the bindings to the axis resolver.
    CHECK_EQ(axis_resolver::GetBinding("SteerAxis"), std::string("wheel-axis:lX"));

    std::error_code ec;
    std::filesystem::remove(path, ec);
    Load();
    const Config d;
    CHECK_EQ(Current().input.steerGainPct, d.input.steerGainPct);
    CHECK(Current().actionBindings.empty());
    testsupport::ResetConfigToDefaults();
}

// ---- Concurrent readers (review 2026-11-02, P1) -------------------------------

TEST_CASE(config_readers_never_see_a_torn_snapshot)
{
    // One writer changes two fields together in each update; readers on
    // other threads (like the pump and the game thread) must always see
    // the pair match. Before snapshots were immutable, two quick updates
    // could rewrite the Config a reader was still copying.
    testsupport::ResetConfigToDefaults();
    std::atomic<bool> done{false};
    std::atomic<int>  torn{0};
    std::atomic<long> reads{0};

    auto reader = [&] {
        while (!done.load(std::memory_order_acquire))
        {
            const Config c = Current();
            auto it = c.actionBindings.find("Race");
            const std::string bound = (it == c.actionBindings.end()) ? std::string() : it->second;
            if (bound != c.music.processName) torn.fetch_add(1);
            reads.fetch_add(1);
        }
    };
    std::thread r1(reader), r2(reader);

    for (int i = 0; i < 1500; ++i)
    {
        // Lengths vary so a torn copy also changes string allocations.
        const std::string v(static_cast<size_t>(16 + (i * 37) % 200), static_cast<char>('a' + i % 26));
        Mutate([&](Config& c) {
            c.music.processName       = v;
            c.actionBindings["Race"]  = v;
            c.actionBindings["Pad" + std::to_string(i % 7)] = v;
        });
    }
    done.store(true, std::memory_order_release);
    r1.join();
    r2.join();

    CHECK_EQ(torn.load(), 0);
    CHECK(reads.load() > 0);
    testsupport::ResetConfigToDefaults();
}
