#include "fake_logitech_sdk.h"

#include <LogitechSteeringWheelLib.h>

#include <cwchar>

namespace fakelogi
{
    State& S()
    {
        static State s;
        return s;
    }

    void Reset()
    {
        S() = State{};
    }

    namespace
    {
        bool Rec(const char* name, int idx = -1, float a = 0.f, float b = 0.f, float c = 0.f)
        {
            S().calls.push_back({ name, idx, a, b, c });
            return true;
        }

        bool Connected(int idx)
        {
            return S().connectedSlot >= 0 && idx == S().connectedSlot;
        }
    }
}

using fakelogi::S;
using fakelogi::Rec;

bool LogiSteeringInitializeWithWindow(bool, HWND) { Rec("LogiSteeringInitializeWithWindow"); return S().initResult; }
bool LogiSteeringInitialize(bool)                 { Rec("LogiSteeringInitialize"); return S().initResult; }
void LogiSteeringShutdown()                       { Rec("LogiSteeringShutdown"); }

bool LogiSteeringGetSdkVersion(int* major, int* minor, int* build)
{
    *major = 8; *minor = 75; *build = 30;
    return true;
}

bool LogiUpdate() { return true; }

DIJOYSTATE2* LogiGetState(const int index)
{
    return fakelogi::Connected(index) ? &S().joy : nullptr;
}

bool LogiGetFriendlyProductName(const int index, wchar_t* buffer, int bufferSize)
{
    if (!fakelogi::Connected(index)) return false;
    std::wcsncpy(buffer, S().productName.c_str(), static_cast<size_t>(bufferSize) - 1);
    buffer[bufferSize - 1] = L'\0';
    return true;
}

bool LogiIsConnected(const int index)                  { return fakelogi::Connected(index); }
bool LogiIsDeviceConnected(const int index, const int) { return fakelogi::Connected(index) && S().isWheel; }
bool LogiIsManufacturerConnected(const int index, const int) { return fakelogi::Connected(index); }
bool LogiHasForceFeedback(const int index)             { return fakelogi::Connected(index) && S().hasFfb; }

bool LogiPlaySpringForce(const int index, const int offset, const int saturation, const int coefficient)
{
    return Rec("LogiPlaySpringForce", index, static_cast<float>(offset),
               static_cast<float>(saturation), static_cast<float>(coefficient));
}
bool LogiStopSpringForce(const int index)                 { return Rec("LogiStopSpringForce", index); }
bool LogiPlayConstantForce(const int index, const int pct) { return Rec("LogiPlayConstantForce", index, static_cast<float>(pct)); }
bool LogiStopConstantForce(const int index)               { return Rec("LogiStopConstantForce", index); }
bool LogiPlayDamperForce(const int index, const int pct)  { return Rec("LogiPlayDamperForce", index, static_cast<float>(pct)); }
bool LogiStopDamperForce(const int index)                 { return Rec("LogiStopDamperForce", index); }

bool LogiPlaySurfaceEffect(const int index, const int type, const int pct, const int period)
{
    return Rec("LogiPlaySurfaceEffect", index, static_cast<float>(type),
               static_cast<float>(pct), static_cast<float>(period));
}
bool LogiStopSurfaceEffect(const int index) { return Rec("LogiStopSurfaceEffect", index); }
bool LogiPlayCarAirborne(const int index)   { return Rec("LogiPlayCarAirborne", index); }
bool LogiStopCarAirborne(const int index)   { return Rec("LogiStopCarAirborne", index); }

bool LogiSetOperatingRange(const int index, const int range)
{
    return Rec("LogiSetOperatingRange", index, static_cast<float>(range));
}

bool LogiGetOperatingRange(const int index, int& range)
{
    if (!fakelogi::Connected(index) || !S().rangeAnswers) return false;
    range = S().rangeDeg;
    return true;
}

bool LogiPlayLeds(const int index, const float currentRPM, const float firstLed, const float redLine)
{
    return Rec("LogiPlayLeds", index, currentRPM, firstLed, redLine);
}
