#include "plugin.h"
#include "logging.h"
#include "wheel.h"
#include "sources.h"
#include "axis_resolver.h"
#include "input_bindings.h"
#include "vehicle_hook.h"
#include "kbd_hook.h"
#include "config.h"
#include "rtti.h"
#include "audio_monitor.h"
#include "led.h"
#include "mod_settings_seed.h"
#include "trueforce_loader.h"

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>

namespace gwheel
{
    namespace
    {
        std::atomic<bool> g_pumpRunning{false};
        std::thread       g_pumpThread;

        // Convert the Logi SDK's wheel::Snapshot into the canonical
        // sources::Frame that the rest of the plugin consumes. Steering /
        // throttle / brake are routed through axis_resolver against the
        // user's configured action bindings. An unbound action resolves
        // to 0, which means a fresh install with empty actionBindings
        // produces a still wheel and dead pedals until the user binds
        // them via Mod Settings.
        sources::Frame BuildFrame(const wheel::Snapshot& s)
        {
            sources::Frame f;
            f.connected       = s.connected;
            f.axes.steer      = axis_resolver::Resolve("SteerAxis",    s, axis_resolver::Combine::SignedFirst);
            f.axes.throttle   = axis_resolver::Resolve("ThrottleAxis", s, axis_resolver::Combine::UnsignedMax);
            f.axes.brake      = axis_resolver::Resolve("BrakeAxis",    s, axis_resolver::Combine::UnsignedMax);
            f.digital.buttons = s.buttons;
            f.digital.pov     = s.pov;
            return f;
        }

        // The pump runs on a fixed 4 ms schedule. Plain sleep_for(4ms) is
        // rounded up to the system timer resolution (15.6 ms unless some
        // process raised it), which can drop the pump to ~64 Hz and leave the
        // axes the game reads up to ~16 ms stale. A high-resolution waitable
        // timer keeps the period close to 4 ms regardless, and above-normal
        // priority keeps the game's own worker threads from delaying it.
        constexpr auto kPumpPeriod = std::chrono::milliseconds(4);

        class PumpTimer
        {
        public:
            PumpTimer()
                : timer_(CreateWaitableTimerExW(nullptr, nullptr,
                                                CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                                TIMER_ALL_ACCESS))
            {
                if (!timer_)
                    log::WarnF("[gwheel] high-resolution pump timer unavailable (error %lu); falling back to sleep_for",
                               GetLastError());
            }
            ~PumpTimer() { if (timer_) CloseHandle(timer_); }
            PumpTimer(const PumpTimer&) = delete;
            PumpTimer& operator=(const PumpTimer&) = delete;

            void SleepFor(std::chrono::steady_clock::duration d)
            {
                if (timer_)
                {
                    LARGE_INTEGER due;
                    // Negative = relative, in 100 ns units.
                    due.QuadPart = -std::max<long long>(
                        1, std::chrono::duration_cast<std::chrono::nanoseconds>(d).count() / 100);
                    if (SetWaitableTimer(timer_, &due, 0, nullptr, nullptr, FALSE))
                    {
                        WaitForSingleObject(timer_, INFINITE);
                        return;
                    }
                }
                std::this_thread::sleep_for(d);
            }

        private:
            HANDLE timer_;
        };

        void PumpLoop()
        {
            using clock = std::chrono::steady_clock;
            log::Info("[gwheel] pump thread started (250 Hz)");
            if (!SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL))
                log::WarnF("[gwheel] pump SetThreadPriority failed (error %lu)", GetLastError());

            PumpTimer timer;

            // Measured tick period, logged once a minute so the real rate
            // shows up in gwheel-*.log.
            constexpr uint32_t kStatsTicks = 15000; // ~60 s at 250 Hz
            uint32_t           statTicks   = 0;
            clock::duration    statMax{};
            clock::time_point  statStart   = clock::now();
            clock::time_point  lastTick    = statStart;

            clock::time_point next = clock::now();
            while (g_pumpRunning.load(std::memory_order_acquire))
            {
                const clock::time_point tickStart = clock::now();
                statMax = std::max(statMax, tickStart - lastTick);
                lastTick = tickStart;
                if (++statTicks == kStatsTicks)
                {
                    const double avgMs = std::chrono::duration<double, std::milli>(tickStart - statStart).count()
                                       / kStatsTicks;
                    log::InfoF("[gwheel] pump period avg=%.2f ms max=%.2f ms over %u ticks",
                               avgMs, std::chrono::duration<double, std::milli>(statMax).count(), kStatsTicks);
                    statTicks = 0;
                    statMax   = {};
                    statStart = tickStart;
                }

                wheel::Pump();
                const sources::Frame frame = BuildFrame(wheel::CurrentSnapshot());
                sources::Publish(frame);
                if (frame.connected)
                    input_bindings::OnTick(frame);
                else
                    input_bindings::ReleaseHeld();  // no release edges arrive while unplugged
                input_bindings::ServicePendingTaps();

                // Fixed-rate schedule. If a tick overran (stall, breakpoint,
                // suspend), restart the schedule from now instead of
                // bursting to catch up.
                next += kPumpPeriod;
                const clock::time_point now = clock::now();
                if (next <= now)
                    next = now;
                else
                    timer.SleepFor(next - now);
            }
            log::Info("[gwheel] pump thread stopped");
        }
    }

    PluginContext& Ctx()
    {
        static PluginContext ctx;
        return ctx;
    }

    void OnLoad(RED4ext::v1::PluginHandle aHandle, const RED4ext::v1::Sdk* aSdk)
    {
        auto& ctx = Ctx();
        ctx.handle = aHandle;
        ctx.sdk = aSdk;

        log::InfoF("[gwheel] ========================================");
        log::InfoF("[gwheel] loaded v%s", kVersionString);
        log::InfoF("[gwheel] ========================================");

        log::Info("[gwheel] step 1/10: loading config");
        config::Load();

        // MUST run before mod_settings reads its user.ini in ProcessScriptData.
        // All RED4ext plugin OnLoads complete before script processing begins,
        // so this ordering is safe regardless of plugin load order.
        log::Info("[gwheel] step 2/10: probing attached wheel + seeding mod_settings/user.ini");
        mod_settings_seed::Run();

        log::Info("[gwheel] step 3/10: registering redscript natives");
        rtti::Register();

        // TrueForce loader MUST run before wheel::Init so the backend
        // factory's TrueForceApplicable() predicate sees IsActive()==true
        // when the user has opted in. Otherwise the factory always falls
        // through to LegacyBackend regardless of trueForce.enabled.
        log::Info("[gwheel] step 4/10: probing G HUB for TrueForce runtime");
        trueforce::Initialize();

        log::Info("[gwheel] step 5/10: selecting wheel backend + initializing");
        wheel::Init();

        log::Info("[gwheel] step 6/10: installing vehicle-input detour (hash-resolved)");
        vehicle_hook::Init();

        log::Info("[gwheel] step 7/10: installing on-foot keyboard hook (G HUB injection filter)");
        kbd_hook::Install();

        log::Info("[gwheel] step 8/10: starting 250 Hz pump thread");
        g_pumpRunning.store(true, std::memory_order_release);
        g_pumpThread = std::thread(PumpLoop);

        log::Info("[gwheel] step 9/10: starting WASAPI loopback audio monitor");
        audio_monitor::Init();

        log::Info("[gwheel] step 10/10: starting rev-strip LED controller");
        led::Init();

        log::InfoF("[gwheel] ready: hook=%s kbdhook=%s",
                   vehicle_hook::IsInstalled() ? "installed" : "not-installed",
                   kbd_hook::IsInstalled() ? "installed" : "not-installed");
    }

    void OnUnload()
    {
        log::Info("[gwheel] unloading");

        // Detach the vehicle-input detour FIRST so the game's
        // UpdateVehicleCameraInput stops re-entering us while the rest
        // of the plugin is mid-teardown. The detour reads sources::Frame
        // and writes axes into the vehicle — leaving it live until the
        // end means a game-thread call could land on half-torn-down
        // state when the player quits while in a vehicle.
        vehicle_hook::Shutdown();

        // Then shut LED + audio + pump. LED first so the controller
        // isn't still calling LogiPlayLeds as we tear the SDK down.
        led::Shutdown();
        audio_monitor::Shutdown();

        g_pumpRunning.store(false, std::memory_order_release);
        if (g_pumpThread.joinable()) g_pumpThread.join();
        input_bindings::ServicePendingTaps(/*releaseAll=*/true);
        input_bindings::ReleaseHeld();

        // Defensive zeroing of telemetry sources. Threads that read
        // these are already joined; this just ensures the state is
        // clean if the DLL is later reloaded (RED4ext supports hot-
        // reload of plugins) without leaning on static destruction
        // ordering.
        sources::SetInVehicle(false);
        sources::SetEngineRpmRaw(0.f);
        sources::SetCurrentGear(0);
        sources::ClearEngineProfile();
        sources::SetRadioActive(false);

        kbd_hook::Uninstall();
        trueforce::Shutdown();
        wheel::Shutdown();

        auto& ctx = Ctx();
        ctx.handle = {};
        ctx.sdk = nullptr;
    }
}
