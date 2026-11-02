// input_probe — standalone Logi SDK console tool that prints every button
// edge, POV change, and significant axis delta so we can empirically map
// the physical controls of a connected Logitech wheel to their DInput
// indices. Used to build the per-device input layout for the binding UI.
//
// Usage:
//   input_probe.exe          -> runs until killed (Ctrl+C or external)
//   input_probe.exe 30       -> auto-exits after 30 seconds
//   input_probe.exe prompt   -> interactive prompted pass-through test:
//                               walks through every physical control, for
//                               each one waits for the user to press it
//                               on the wheel, times out after 15s. Writes
//                               a machine-readable summary to
//                               input_probe_results.txt in the CWD. Used
//                               to verify what G HUB's active profile is
//                               and is not passing through to DInput.
//
// Requires G HUB (or LGS) running and the wheel NOT claimed by another
// SDK session (so close CP2077 first).

#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <dinput.h>
#include <conio.h>
#include <LogitechSteeringWheelLib.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <string>
#include <thread>

namespace
{
    const char* PovLabel(DWORD pov)
    {
        if (pov == 0xFFFFFFFF) return "CENTER";
        switch (pov)
        {
        case 0:     return "UP";
        case 4500:  return "UP-RIGHT";
        case 9000:  return "RIGHT";
        case 13500: return "DOWN-RIGHT";
        case 18000: return "DOWN";
        case 22500: return "DOWN-LEFT";
        case 27000: return "LEFT";
        case 31500: return "UP-LEFT";
        default:    return "?";
        }
    }
}

namespace
{
    // ---- Prompted pass-through mode ----------------------------------------
    //
    // For each named step, wait until either:
    //   - any button rising edge or POV direction change arrives, OR
    //   - the user presses Enter in the console (marks SKIPPED), OR
    //   - 15 seconds pass with no input (marks TIMEOUT — treated same as
    //     SKIPPED / suppressed by driver).
    // Records what was observed to a summary file so whoever runs the probe
    // can see whether the Logi SDK is seeing each physical control under
    // the current G HUB profile.

    struct Step { const char* label; };

    constexpr Step kSteps[] = {
        { "Left paddle"          },
        { "Right paddle"         },
        { "A"                    },
        { "B"                    },
        { "X"                    },
        { "Y"                    },
        { "D-pad Up"             },
        { "D-pad Down"           },
        { "D-pad Left"           },
        { "D-pad Right"          },
        { "Start (menu)"         },
        { "Select / View"        },
        { "LSB (left stick click)"  },
        { "RSB (right stick click)" },
        { "Plus (+)"             },
        { "Minus (-)"            },
        { "Scroll click"         },
        { "Scroll CW"            },
        { "Scroll CCW"           },
        { "Xbox / Guide"         },
    };
    constexpr int kStepCount = static_cast<int>(sizeof(kSteps) / sizeof(kSteps[0]));

    struct Probe
    {
        uint32_t buttons = 0;
        DWORD    pov     = 0xFFFFFFFF;
    };

    Probe ReadProbe(int idx)
    {
        Probe p;
        const DIJOYSTATE2* raw = LogiGetState(idx);
        if (!raw) return p;
        for (int i = 0; i < 32; ++i)
            if (raw->rgbButtons[i] & 0x80) p.buttons |= (1u << i);
        p.pov = raw->rgdwPOV[0];
        return p;
    }

    int RunPromptedMode(int idx, const wchar_t* wheelName)
    {
        std::FILE* fp = std::fopen("input_probe_results.txt", "w");
        if (fp)
        {
            std::fprintf(fp, "# input_probe prompted pass-through run\n");
            std::fprintf(fp, "# wheel=%ls (SDK slot %d)\n", wheelName, idx);
            std::fprintf(fp, "# columns: step=<label> outcome=<DETECTED|SKIPPED|TIMEOUT> detail=<what arrived>\n");
            std::fflush(fp);
        }

        std::printf("\n");
        std::printf("===============================================\n");
        std::printf(" PROMPTED PASS-THROUGH TEST\n");
        std::printf("===============================================\n");
        std::printf(" For each step:\n");
        std::printf("   - press the physical control on the wheel, OR\n");
        std::printf("   - press ENTER in this console to skip it.\n");
        std::printf(" Each step auto-skips after 15 seconds.\n");
        std::printf(" Keep your hands off everything else between steps.\n");
        std::printf("===============================================\n\n");

        // Consume any stray keypresses already queued up.
        while (_kbhit()) (void)_getch();

        // Establish baseline from current state (in case something is held).
        LogiUpdate();
        Probe base = ReadProbe(idx);

        int detected = 0, skipped = 0, timedOut = 0;

        for (int s = 0; s < kStepCount; ++s)
        {
            std::printf("[%2d/%d] press: %-28s ", s + 1, kStepCount, kSteps[s].label);
            std::fflush(stdout);

            auto start = std::chrono::steady_clock::now();
            bool gotInput = false;
            bool userSkipped = false;
            std::string detail;

            while (true)
            {
                auto elapsed = std::chrono::steady_clock::now() - start;
                if (elapsed > std::chrono::seconds(15)) break;

                LogiUpdate();
                Probe cur = ReadProbe(idx);

                // Any rising button edge since baseline wins.
                uint32_t rising = cur.buttons & ~base.buttons;
                if (rising)
                {
                    int bit = 0;
                    while (bit < 32 && !(rising & (1u << bit))) ++bit;
                    char buf[96];
                    std::snprintf(buf, sizeof(buf),
                                  "button bit %d (mask 0x%08x)", bit, rising);
                    detail = buf;
                    gotInput = true;

                    // Wait for release so the next step starts clean.
                    while (true)
                    {
                        LogiUpdate();
                        Probe r = ReadProbe(idx);
                        if ((r.buttons & rising) == 0) { base = r; break; }
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    }
                    break;
                }

                // POV direction change (center -> direction).
                if (cur.pov != base.pov && cur.pov != 0xFFFFFFFF)
                {
                    char buf[96];
                    std::snprintf(buf, sizeof(buf),
                                  "POV %lu (%s)", cur.pov, PovLabel(cur.pov));
                    detail = buf;
                    gotInput = true;

                    // Wait for POV to return to center.
                    while (true)
                    {
                        LogiUpdate();
                        Probe r = ReadProbe(idx);
                        if (r.pov == 0xFFFFFFFF) { base = r; break; }
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    }
                    break;
                }

                // Enter key in console = skip this step.
                if (_kbhit())
                {
                    int c = _getch();
                    if (c == '\r' || c == '\n')
                    {
                        userSkipped = true;
                        break;
                    }
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(12));
            }

            const char* outcome;
            if (gotInput)             { outcome = "DETECTED";  ++detected; }
            else if (userSkipped)     { outcome = "SKIPPED";   ++skipped;  }
            else                      { outcome = "TIMEOUT";   ++timedOut; }

            if (gotInput)
                std::printf("-> %s  (%s)\n", outcome, detail.c_str());
            else
                std::printf("-> %s\n", outcome);

            if (fp)
            {
                std::fprintf(fp, "step=\"%s\" outcome=%s detail=\"%s\"\n",
                             kSteps[s].label, outcome, detail.c_str());
                std::fflush(fp);
            }
        }

        std::printf("\n===============================================\n");
        std::printf(" summary: DETECTED=%d SKIPPED=%d TIMEOUT=%d (of %d)\n",
                    detected, skipped, timedOut, kStepCount);
        std::printf("===============================================\n");
        std::printf(" results file: input_probe_results.txt\n");
        std::printf("\nPress Enter to close this window...\n");
        std::fflush(stdout);

        if (fp)
        {
            std::fprintf(fp, "# summary: DETECTED=%d SKIPPED=%d TIMEOUT=%d\n",
                         detected, skipped, timedOut);
            std::fclose(fp);
        }

        // Drain any buffered input, then wait for Enter.
        while (_kbhit()) (void)_getch();
        int c;
        while ((c = std::getchar()) != EOF && c != '\n') {}
        return 0;
    }
}

int main(int argc, char** argv)
{
    long long maxSeconds = 0; // 0 == infinite
    bool prompted = false;
    if (argc > 1)
    {
        if (std::strcmp(argv[1], "prompt") == 0) prompted = true;
        else                                     maxSeconds = std::atoll(argv[1]);
    }

    // Hidden top-level window for DInput acquisition.
    HINSTANCE hinst = GetModuleHandleW(nullptr);
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = hinst;
    wc.lpszClassName = L"gwheel_input_probe";
    RegisterClassExW(&wc);
    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"input_probe",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 400, 300,
        nullptr, nullptr, hinst, nullptr);

    if (!LogiSteeringInitializeWithWindow(false, hwnd))
    {
        std::printf("LogiSteeringInitialize failed. Close CP2077 and make sure G HUB is running.\n");
        return 1;
    }

    int idx = -1;
    for (int i = 0; i < 60 && idx < 0; ++i)
    {
        LogiUpdate();
        for (int j = 0; j < LOGI_MAX_CONTROLLERS; ++j)
        {
            if (LogiIsConnected(j) && LogiIsDeviceConnected(j, LOGI_DEVICE_TYPE_WHEEL)) { idx = j; break; }
        }
        if (idx < 0) std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    if (idx < 0)
    {
        std::printf("no wheel detected within 3s\n");
        LogiSteeringShutdown();
        return 2;
    }

    wchar_t name[256] = {};
    LogiGetFriendlyProductName(idx, name, 256);
    std::wprintf(L"bound to %s (slot %d)\n", name, idx);
    std::fflush(stdout);

    if (prompted)
    {
        return RunPromptedMode(idx, name);
    }

    std::printf("---\n");
    std::printf("press each physical control once with a ~1s pause.\n");
    std::printf("axis noise gate = 1000 counts; steering/pedals are suppressed.\n");
    std::printf("---\n");
    std::fflush(stdout);

    auto now_ms = []() {
        auto t = std::chrono::steady_clock::now().time_since_epoch();
        return std::chrono::duration_cast<std::chrono::milliseconds>(t).count();
    };
    const long long start = now_ms();

    uint32_t prevButtons = 0;
    DWORD prevPov = 0;
    LONG prevLZ = 0, prevLRx = 0, prevLRy = 0, prevSl0 = 0, prevSl1 = 0;
    bool firstTick = true;

    while (true)
    {
        LogiUpdate();
        const DIJOYSTATE2* raw = LogiGetState(idx);
        const long long t = now_ms() - start;
        if (maxSeconds > 0 && t > maxSeconds * 1000) break;
        if (!raw) { std::this_thread::sleep_for(std::chrono::milliseconds(16)); continue; }

        uint32_t bits = 0;
        for (int i = 0; i < 32; ++i)
            if (raw->rgbButtons[i] & 0x80) bits |= (1u << i);

        const DWORD pov = raw->rgdwPOV[0];

        if (firstTick)
        {
            firstTick = false;
            prevButtons = bits;
            prevPov = pov;
            prevLZ = raw->lZ;
            prevLRx = raw->lRx;
            prevLRy = raw->lRy;
            prevSl0 = raw->rglSlider[0];
            prevSl1 = raw->rglSlider[1];
            std::printf("t=%lldms  INITIAL  buttons=0x%08X  pov=%u(%s)  "
                        "lZ=%ld lRx=%ld lRy=%ld sl0=%ld sl1=%ld\n",
                        t, bits, pov, PovLabel(pov),
                        raw->lZ, raw->lRx, raw->lRy,
                        raw->rglSlider[0], raw->rglSlider[1]);
            std::fflush(stdout);
        }
        else
        {
            uint32_t rising  = bits & ~prevButtons;
            uint32_t falling = prevButtons & ~bits;
            for (int i = 0; i < 32; ++i)
            {
                if (rising  & (1u << i)) std::printf("t=%lldms  BUTTON %2d  DOWN\n", t, i);
                if (falling & (1u << i)) std::printf("t=%lldms  BUTTON %2d  UP\n",   t, i);
            }
            if (pov != prevPov)
                std::printf("t=%lldms  POV  %u(%s) -> %u(%s)\n",
                            t, prevPov, PovLabel(prevPov), pov, PovLabel(pov));

            auto axisCheck = [&](const char* label, LONG cur, LONG& prev) {
                LONG diff = cur - prev;
                if (diff > 1000 || diff < -1000)
                {
                    std::printf("t=%lldms  AXIS %s  %ld -> %ld (delta %+ld)\n",
                                t, label, prev, cur, diff);
                    prev = cur;
                }
            };
            axisCheck("lZ", raw->lZ, prevLZ);
            axisCheck("lRx", raw->lRx, prevLRx);
            axisCheck("lRy", raw->lRy, prevLRy);
            axisCheck("slider0", raw->rglSlider[0], prevSl0);
            axisCheck("slider1", raw->rglSlider[1], prevSl1);

            prevButtons = bits;
            prevPov = pov;

            if (rising || falling || pov != prevPov) std::fflush(stdout);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    LogiSteeringShutdown();
    std::printf("done.\n");
    return 0;
}
