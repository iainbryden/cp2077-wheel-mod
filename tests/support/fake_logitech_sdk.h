#pragma once

// Link-time replacement for LogitechSteeringWheelLib. The test executable
// never links Logitech's import library; fake_logitech_sdk.cpp defines
// every Logi* function the plugin calls and records each call here.

#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <dinput.h>

#include <string>
#include <vector>

namespace fakelogi
{
    struct Call
    {
        std::string name;
        int         index = -1;
        float       a = 0.f, b = 0.f, c = 0.f;
    };

    struct State
    {
        bool         initResult      = true;   // LogiSteeringInitialize*
        int          connectedSlot   = 0;      // -1 = nothing connected
        bool         isWheel         = true;   // LogiIsDeviceConnected(WHEEL)
        bool         hasFfb          = true;
        bool         rangeAnswers    = false;  // LogiGetOperatingRange succeeds?
        int          rangeDeg        = 900;
        std::wstring productName     = L"G29 Driving Force Racing Wheel";
        DIJOYSTATE2  joy{};
        std::vector<Call> calls;

        int Count(const char* n) const
        {
            int k = 0;
            for (const auto& c : calls) if (c.name == n) ++k;
            return k;
        }
        const Call* Last(const char* n) const
        {
            for (auto it = calls.rbegin(); it != calls.rend(); ++it)
                if (it->name == n) return &*it;
            return nullptr;
        }
    };

    State& S();
    void   Reset();
}
