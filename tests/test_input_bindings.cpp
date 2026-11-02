#include "testfw.h"
#include "support/fake_backend.h"
#include "support/test_hooks.h"

#include <windows.h>

// Capture every synthetic key / mouse event instead of sending it to
// Windows: a test run must never type into whatever window has focus.
namespace testsupport_input
{
    UINT WINAPI FakeSendInput(UINT count, LPINPUT inputs, int /*size*/)
    {
        for (UINT i = 0; i < count; ++i)
        {
            testsupport::SentInput s;
            const INPUT& in = inputs[i];
            if (in.type == INPUT_KEYBOARD)
            {
                s.keyboard  = true;
                s.vk        = in.ki.wVk;
                s.keyUp     = (in.ki.dwFlags & KEYEVENTF_KEYUP) != 0;
                s.extraInfo = in.ki.dwExtraInfo;
            }
            else
            {
                s.mouseFlags = in.mi.dwFlags;
                s.mouseData  = static_cast<int32_t>(in.mi.mouseData);
                s.extraInfo  = in.mi.dwExtraInfo;
            }
            testsupport::SentInputs().push_back(s);
        }
        return count;
    }
}

#define SendInput testsupport_input::FakeSendInput
#include "input_bindings.cpp"
#undef SendInput

// The keyboard hook's vehicle-only key list must agree with the dispatch
// table above, so both live in this test file.
#include "kbd_hook.cpp"

#include <set>
#include <thread>

using namespace gwheel;
using namespace gwheel::input_bindings;
using testsupport::SentInputs;

namespace testsupport
{
    std::vector<SentInput>& SentInputs()
    {
        static std::vector<SentInput> v;
        return v;
    }
}

namespace
{
    void ResetBindingState()
    {
        ServicePendingTaps(true);
        Clear();
        CancelCapture();
        auto& st = S();
        st.prevButtons = 0;
        st.prevPov     = 0xFFFFFFFF;
        st.havePrev    = false;
        SentInputs().clear();
    }

    sources::Frame FrameWith(uint32_t buttons, uint16_t pov = 0xFFFF)
    {
        sources::Frame f;
        f.connected       = true;
        f.digital.buttons = buttons;
        f.digital.pov     = pov;
        return f;
    }

    // Seeds the edge detector with an idle frame, as the first pump tick does.
    void Prime()
    {
        OnTick(FrameWith(0));
        SentInputs().clear();
    }

    bool ParseStr(const std::string& s, ParsedSource& out)
    {
        return ParseToken(s.data(), s.size(), out);
    }

    // Common setup: defaults, in a vehicle, edge detector primed.
    struct Fixture
    {
        testsupport::FakeBackend backend;
        Fixture()
        {
            testsupport::InstallBackend(&backend);
            testsupport::ResetConfigToDefaults();
            sources::SetInVehicle(true);
            ResetBindingState();
            Prime();
        }
        ~Fixture()
        {
            ResetBindingState();
            sources::SetInVehicle(false);
            testsupport::ResetConfigToDefaults();
            testsupport::InstallBackend(nullptr);
        }
    };
}

// ---- Tables -------------------------------------------------------------

TEST_CASE(bindings_action_names_round_trip)
{
    for (int32_t i = 1; i < kActionCount; ++i)
        CHECK_EQ(ActionFromName(kActionNames[static_cast<size_t>(i)]), i);
    CHECK_EQ(ActionFromName("RearViewCamera"), static_cast<int32_t>(CameraCycleBackward));
    CHECK_EQ(ActionFromName("SteerAxis"), static_cast<int32_t>(None));
    CHECK_EQ(ActionFromName(""), static_cast<int32_t>(None));
    CHECK_EQ(ActionFromName("horn"), static_cast<int32_t>(None));  // case-sensitive
}

TEST_CASE(bindings_action_names_are_unique)
{
    for (size_t i = 0; i < kActionNames.size(); ++i)
        for (size_t j = i + 1; j < kActionNames.size(); ++j)
            CHECK(std::strcmp(kActionNames[i], kActionNames[j]) != 0);
}

TEST_CASE(bindings_every_action_has_a_dispatch_entry)
{
    CHECK(kDispatch[0].kind == DispatchKind::None);
    for (int32_t i = 1; i < kActionCount; ++i)
    {
        const auto& e = kDispatch[static_cast<size_t>(i)];
        CHECK(e.kind != DispatchKind::None);
        CHECK(e.label != nullptr && e.label[0] != '\0');
        if (e.kind == DispatchKind::Keyboard)      CHECK(e.vk != 0);
        if (e.kind == DispatchKind::MouseButton)   CHECK(e.mouseDownFlag != 0 && e.mouseUpFlag != 0);
        if (e.kind == DispatchKind::MouseWheel)    CHECK(e.wheelDelta != 0);
        if (e.kind == DispatchKind::VehicleNative) CHECK(e.vehicleMethodName != nullptr);
    }
    CHECK_EQ(std::string(ActionLabel(-1)), std::string("?"));
    CHECK_EQ(std::string(ActionLabel(kActionCount)), std::string("?"));
}

TEST_CASE(bindings_menu_and_call_vehicle_work_on_foot)
{
    // Opening menus and summoning the car must not need a vehicle.
    for (Action a : { CallVehicle, QuickSave, OpenMap, OpenJournal, OpenInventory, OpenPhone,
                      OpenPerks, OpenCrafting, Pause, MenuConfirm, MenuCancel,
                      MenuUp, MenuDown, MenuLeft, MenuRight })
        CHECK(kDispatch[static_cast<size_t>(a)].scope == ActionScope::AnyContext);
    for (Action a : { Horn, Handbrake, ExitVehicle, ShootPrimary, RadioNext })
        CHECK(kDispatch[static_cast<size_t>(a)].scope == ActionScope::VehicleOnly);
}

// ---- Token parsing ------------------------------------------------------

TEST_CASE(bindings_parse_button_tokens)
{
    ParsedSource s;
    REQUIRE(ParseStr("wheel:0", s));
    CHECK(s.kind == ParsedSource::Kind::WheelButton);
    CHECK_EQ(s.value, 0u);
    REQUIRE(ParseStr("wheel:31", s));
    CHECK_EQ(s.value, 31u);
    CHECK(!ParseStr("wheel:32", s));       // beyond the 32-bit button mask
    CHECK(!ParseStr("wheel:", s));
    CHECK(!ParseStr("wheel:-1", s));
    CHECK(!ParseStr("wheel-axis:lX", s));
    CHECK(!ParseStr("keyboard:KEY_H", s));
    CHECK(!ParseStr("", s));
}

TEST_CASE(bindings_parse_pov_tokens)
{
    ParsedSource s;
    REQUIRE(ParseStr("wheel-pov:18000", s));
    CHECK(s.kind == ParsedSource::Kind::WheelPov);
    CHECK_EQ(s.value, 18000u);
    REQUIRE(ParseStr("wheel-pov:0", s));
    CHECK_EQ(s.value, 0u);
    CHECK(!ParseStr("wheel-pov:", s));
    CHECK(!ParseStr("wheel-pov:up", s));
}

TEST_CASE(bindings_source_activity)
{
    ParsedSource btn{ ParsedSource::Kind::WheelButton, 4 };
    CHECK(IsSourceActive(btn, 1u << 4, 0xFFFF));
    CHECK(!IsSourceActive(btn, 1u << 5, 0xFFFF));
    ParsedSource pov{ ParsedSource::Kind::WheelPov, 9000 };
    CHECK(IsSourceActive(pov, 0, 9000));
    CHECK(!IsSourceActive(pov, 0, 0xFFFF));
    CHECK(AnySourceActive({ btn, pov }, 0, 9000));
    CHECK(!AnySourceActive({}, 0xFFFFFFFF, 9000));
}

TEST_CASE(bindings_set_action_binding_parses_and_ignores)
{
    ResetBindingState();
    SetActionBinding("Horn", "wheel:0|keyboard:KEY_Z|wheel-pov:0|wheel-axis:lX");
    SetActionBinding("SteerAxis", "wheel-axis:lX");   // axis-only key: not stored here
    SetActionBinding("", "wheel:1");
    {
        auto& st = S();
        std::lock_guard lk(st.mtx);
        REQUIRE(st.bindings.count("Horn") == 1);
        CHECK_EQ(st.bindings["Horn"].sources.size(), static_cast<size_t>(2));
        CHECK_EQ(st.bindings.count("SteerAxis"), static_cast<size_t>(0));
        CHECK_EQ(st.bindings.size(), static_cast<size_t>(1));
    }
    SetActionBinding("Horn", "");
    {
        auto& st = S();
        std::lock_guard lk(st.mtx);
        CHECK(st.bindings.empty());
    }
}

// ---- Dispatch -------------------------------------------------------------

TEST_CASE(bindings_first_tick_only_seeds_edge_state)
{
    testsupport::FakeBackend backend;
    testsupport::InstallBackend(&backend);
    testsupport::ResetConfigToDefaults();
    sources::SetInVehicle(true);
    ResetBindingState();
    SetActionBinding("Horn", "wheel:0");
    // Button already held on the very first tick: no press is reported.
    OnTick(FrameWith(1u << 0));
    CHECK(SentInputs().empty());
    ResetBindingState();
    sources::SetInVehicle(false);
    testsupport::InstallBackend(nullptr);
}

TEST_CASE(bindings_hold_action_follows_button)
{
    Fixture fx;
    SetActionBinding("Horn", "wheel:0");
    OnTick(FrameWith(1u << 0));
    REQUIRE(SentInputs().size() == 1);
    CHECK(SentInputs()[0].keyboard);
    CHECK_EQ(SentInputs()[0].vk, static_cast<uint16_t>('Z'));
    CHECK(!SentInputs()[0].keyUp);
    CHECK_EQ(SentInputs()[0].extraInfo, kbd_hook::kExtraInfoTag);

    OnTick(FrameWith(1u << 0));   // still held: nothing new
    CHECK_EQ(SentInputs().size(), static_cast<size_t>(1));

    OnTick(FrameWith(0));
    REQUIRE(SentInputs().size() == 2);
    CHECK(SentInputs()[1].keyUp);
}

TEST_CASE(bindings_tap_action_releases_after_hold_time)
{
    Fixture fx;
    SetActionBinding("CameraCycleForward", "wheel:1");
    OnTick(FrameWith(1u << 1));
    REQUIRE(SentInputs().size() == 1);
    CHECK_EQ(SentInputs()[0].vk, static_cast<uint16_t>('Q'));
    CHECK(!SentInputs()[0].keyUp);

    // Releasing the button does not send the key-up early.
    OnTick(FrameWith(0));
    ServicePendingTaps();
    CHECK_EQ(SentInputs().size(), static_cast<size_t>(1));

    std::this_thread::sleep_for(std::chrono::milliseconds(45));
    ServicePendingTaps();
    REQUIRE(SentInputs().size() == 2);
    CHECK(SentInputs()[1].keyUp);
    CHECK_EQ(SentInputs()[1].vk, static_cast<uint16_t>('Q'));

    ServicePendingTaps();
    CHECK_EQ(SentInputs().size(), static_cast<size_t>(2));
}

TEST_CASE(bindings_tap_repress_releases_previous_tap_first)
{
    Fixture fx;
    SetActionBinding("CameraReset", "wheel:2");
    OnTick(FrameWith(1u << 2));
    OnTick(FrameWith(0));
    OnTick(FrameWith(1u << 2));   // second press inside the 30 ms hold
    REQUIRE(SentInputs().size() == 3);
    CHECK(!SentInputs()[0].keyUp);
    CHECK(SentInputs()[1].keyUp);
    CHECK(!SentInputs()[2].keyUp);
    ServicePendingTaps(true);
    REQUIRE(SentInputs().size() == 4);
    CHECK(SentInputs()[3].keyUp);
}

TEST_CASE(bindings_release_all_flushes_pending_taps)
{
    Fixture fx;
    SetActionBinding("OpenMap", "wheel:3");
    SetActionBinding("OpenJournal", "wheel:4");
    OnTick(FrameWith((1u << 3) | (1u << 4)));
    CHECK_EQ(SentInputs().size(), static_cast<size_t>(2));
    ServicePendingTaps(true);
    REQUIRE(SentInputs().size() == 4);
    CHECK(SentInputs()[2].keyUp);
    CHECK(SentInputs()[3].keyUp);
}

TEST_CASE(bindings_vehicle_only_actions_are_suppressed_on_foot)
{
    Fixture fx;
    sources::SetInVehicle(false);
    SetActionBinding("Horn", "wheel:0");
    SetActionBinding("OpenMap", "wheel:1");
    OnTick(FrameWith((1u << 0) | (1u << 1)));
    REQUIRE(SentInputs().size() == 1);              // only the map key
    CHECK_EQ(SentInputs()[0].vk, static_cast<uint16_t>('M'));
}

TEST_CASE(bindings_pov_binding_fires_on_direction)
{
    Fixture fx;
    SetActionBinding("CameraCycleForward", "wheel-pov:0");
    OnTick(FrameWith(0, 9000));       // wrong direction
    CHECK(SentInputs().empty());
    OnTick(FrameWith(0, 0));          // up
    REQUIRE(SentInputs().size() == 1);
    CHECK_EQ(SentInputs()[0].vk, static_cast<uint16_t>('Q'));
}

TEST_CASE(bindings_multiple_sources_are_or_combined)
{
    Fixture fx;
    SetActionBinding("Handbrake", "wheel:4|wheel:5");
    OnTick(FrameWith(1u << 4));
    OnTick(FrameWith((1u << 4) | (1u << 5)));
    OnTick(FrameWith(1u << 5));
    CHECK_EQ(SentInputs().size(), static_cast<size_t>(1));  // still held
    OnTick(FrameWith(0));
    REQUIRE(SentInputs().size() == 2);
    CHECK_EQ(SentInputs()[0].vk, static_cast<uint16_t>(VK_SPACE));
    CHECK(SentInputs()[1].keyUp);
}

TEST_CASE(bindings_mouse_button_and_wheel_actions)
{
    Fixture fx;
    SetActionBinding("ShootPrimary", "wheel:6");
    OnTick(FrameWith(1u << 6));
    OnTick(FrameWith(0));
    REQUIRE(SentInputs().size() == 2);
    CHECK(!SentInputs()[0].keyboard);
    CHECK_EQ(SentInputs()[0].mouseFlags, static_cast<uint32_t>(MOUSEEVENTF_LEFTDOWN));
    CHECK_EQ(SentInputs()[1].mouseFlags, static_cast<uint32_t>(MOUSEEVENTF_LEFTUP));
    CHECK_EQ(SentInputs()[0].extraInfo, kbd_hook::kExtraInfoTag);

    SentInputs().clear();
    SetActionBinding("PrevWeapon", "wheel:7");
    OnTick(FrameWith(1u << 7));
    ServicePendingTaps(true);
    REQUIRE(SentInputs().size() == 1);              // a wheel notch has no "up"
    CHECK_EQ(SentInputs()[0].mouseFlags, static_cast<uint32_t>(MOUSEEVENTF_WHEEL));
    CHECK_EQ(SentInputs()[0].mouseData, -WHEEL_DELTA);
}

TEST_CASE(bindings_vehicle_native_without_vehicle_is_a_no_op)
{
    Fixture fx;
    SetActionBinding("RadioNext", "wheel:8");
    OnTick(FrameWith(1u << 8));
    ServicePendingTaps(true);
    CHECK(SentInputs().empty());
}

TEST_CASE(bindings_disabled_input_dispatches_nothing)
{
    Fixture fx;
    config::SetInputEnabled(false);
    SetActionBinding("Horn", "wheel:0");
    OnTick(FrameWith(1u << 0));
    CHECK(SentInputs().empty());
}

TEST_CASE(bindings_keyboard_only_binding_dispatches_nothing)
{
    Fixture fx;
    SetActionBinding("Horn", "keyboard:KEY_Z");
    OnTick(FrameWith(0xFFFFFFFFu));
    CHECK(SentInputs().empty());
}

// ---- Capture --------------------------------------------------------------

TEST_CASE(bindings_capture_button_reports_new_press_and_wakes_ui)
{
    Fixture fx;
    fx.backend.snap.buttons = 1u << 2;     // already held when capture starts
    BeginCapture("button");
    CHECK(IsCapturing());
    CHECK(PollLatestCapture().empty());

    OnTick(FrameWith(1u << 2));            // held button is ignored
    CHECK(PollLatestCapture().empty());

    OnTick(FrameWith((1u << 2) | (1u << 9)));
    // F24 down + up wakes the redscript listening UI.
    REQUIRE(SentInputs().size() == 2);
    CHECK_EQ(SentInputs()[0].vk, static_cast<uint16_t>(VK_F24));
    CHECK(SentInputs()[1].keyUp);

    CHECK_EQ(PollLatestCapture(), std::string("wheel:9"));
    CHECK(!IsCapturing());
    CHECK(PollLatestCapture().empty());
}

TEST_CASE(bindings_capture_button_reports_pov)
{
    Fixture fx;
    BeginCapture("button");
    OnTick(FrameWith(0, 27000));
    CHECK_EQ(PollLatestCapture(), std::string("wheel-pov:27000"));
}

TEST_CASE(bindings_capture_suppresses_bound_actions)
{
    Fixture fx;
    SetActionBinding("Horn", "wheel:0");
    BeginCapture(nullptr);                 // null hint means button capture
    OnTick(FrameWith(1u << 0));
    // Only the wake-up keypress, never the horn.
    for (const auto& s : SentInputs()) CHECK_EQ(s.vk, static_cast<uint16_t>(VK_F24));
    CHECK_EQ(PollLatestCapture(), std::string("wheel:0"));

    // After capture, holding the button fires nothing new. Documents current
    // behaviour: letting go then sends an unpaired horn key-up, because the
    // press happened while capture was armed.
    SentInputs().clear();
    OnTick(FrameWith(1u << 0));
    CHECK(SentInputs().empty());
    OnTick(FrameWith(0));
    REQUIRE(SentInputs().size() == 1);
    CHECK(SentInputs()[0].keyUp);
    CHECK_EQ(SentInputs()[0].vk, static_cast<uint16_t>('Z'));
}

TEST_CASE(bindings_capture_axis_waits_for_a_stable_leader)
{
    Fixture fx;
    BeginCapture("axis");
    // Throttle bumps first, then the wheel turns further and stays there.
    fx.backend.snap.axes[wheel::Axis_lY] = 0.3f;
    for (int i = 0; i < 20; ++i) OnTick(FrameWith(0));
    CHECK(PollLatestCapture().empty());    // lY led, but not for long enough

    fx.backend.snap.axes[wheel::Axis_lX] = 0.9f;
    for (int i = 0; i < 49; ++i) OnTick(FrameWith(0));
    CHECK(PollLatestCapture().empty());
    for (int i = 0; i < 2; ++i) OnTick(FrameWith(0));
    CHECK_EQ(PollLatestCapture(), std::string("wheel-axis:lX"));
}

TEST_CASE(bindings_capture_axis_ignores_small_movements)
{
    Fixture fx;
    BeginCapture("axis");
    fx.backend.snap.axes[wheel::Axis_lRz] = 0.2f;   // under the 0.25 threshold
    fx.backend.snap.buttons = 1u << 3;               // buttons are ignored in axis mode
    for (int i = 0; i < 120; ++i) OnTick(FrameWith(1u << 3));
    CHECK(PollLatestCapture().empty());
    CHECK(IsCapturing());
    CancelCapture();
    CHECK(!IsCapturing());
}

TEST_CASE(bindings_cancel_capture_discards_result)
{
    Fixture fx;
    BeginCapture("button");
    OnTick(FrameWith(1u << 1));
    CancelCapture();
    CHECK(PollLatestCapture().empty());
    CHECK(!IsCapturing());
}

// ---- Keyboard hook (on-foot filter for G HUB's injected keys) ---------------

namespace
{
    LRESULT HookKey(DWORD vk, bool injected, uintptr_t extraInfo = 0, int nCode = HC_ACTION)
    {
        KBDLLHOOKSTRUCT info{};
        info.vkCode      = vk;
        info.flags       = injected ? LLKHF_INJECTED : 0;
        info.dwExtraInfo = extraInfo;
        return kbd_hook::HookProc(nCode, WM_KEYDOWN, reinterpret_cast<LPARAM>(&info));
    }
}

TEST_CASE(kbd_hook_swallows_foreign_injected_vehicle_keys_on_foot)
{
    sources::SetInVehicle(false);
    CHECK_EQ(HookKey('Z', true), static_cast<LRESULT>(1));
    CHECK_EQ(HookKey(VK_SPACE, true), static_cast<LRESULT>(1));
}

TEST_CASE(kbd_hook_passes_everything_else)
{
    sources::SetInVehicle(false);
    CHECK_EQ(HookKey('Z', false), static_cast<LRESULT>(0));                          // physical key
    CHECK_EQ(HookKey('Z', true, kbd_hook::kExtraInfoTag), static_cast<LRESULT>(0));  // our own event
    CHECK_EQ(HookKey('M', true), static_cast<LRESULT>(0));                           // menu key
    CHECK_EQ(HookKey('Z', true, 0, -1), static_cast<LRESULT>(0));                    // not HC_ACTION
    sources::SetInVehicle(true);
    CHECK_EQ(HookKey('Z', true), static_cast<LRESULT>(0));                           // driving
    sources::SetInVehicle(false);
}

TEST_CASE(kbd_hook_key_list_matches_vehicle_only_keyboard_actions)
{
    // Every key a vehicle-only action presses must be filtered on foot, and
    // nothing else: an extra entry would eat a key some on-foot action needs.
    std::set<DWORD> fromTable;
    for (int32_t i = 1; i < kActionCount; ++i)
    {
        const auto& e = kDispatch[static_cast<size_t>(i)];
        if (e.kind == DispatchKind::Keyboard && e.scope == ActionScope::VehicleOnly)
            fromTable.insert(e.vk);
    }
    std::set<DWORD> fromHook;
    for (DWORD vk = 1; vk < 256; ++vk)
        if (kbd_hook::IsVehicleOnlyKey(vk)) fromHook.insert(vk);
    CHECK(fromTable == fromHook);
    CHECK_EQ(fromHook.size(), static_cast<size_t>(15));
}

TEST_CASE(kbd_hook_is_not_installed_in_tests)
{
    CHECK(!kbd_hook::IsInstalled());
}
