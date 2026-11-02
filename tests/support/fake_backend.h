#pragma once

// Recording stand-in for a wheel backend. Installed into wheel.cpp's
// g_backend through testsupport::InstallBackend, so every wheel:: call the
// plugin makes (FFB effects, LEDs, snapshots) lands here instead of on the
// Logitech SDK.

#include "wheel_backend.h"

#include <string>
#include <vector>

namespace testsupport
{
    struct Call
    {
        std::string name;
        float       a = 0.f;
        float       b = 0.f;
        float       c = 0.f;
    };

    class FakeBackend final : public gwheel::wheel_backend::IWheelBackend
    {
    public:
        using Vendor       = gwheel::wheel_backend::Vendor;
        using BackendKind  = gwheel::wheel_backend::BackendKind;
        using DeviceInfo   = gwheel::wheel_backend::DeviceInfo;
        using Capabilities = gwheel::wheel_backend::Capabilities;

        bool                     open      = true;
        bool                     ffb       = true;
        int                      rangeDeg  = 540;
        gwheel::wheel::Snapshot  snap{};
        DeviceInfo               info{};
        Capabilities             caps{};
        std::vector<Call>        calls;
        std::vector<float>       tfSamples;

        FakeBackend()
        {
            caps.ffbConstant = caps.ffbSpring = caps.ffbDamper = true;
            caps.ffbPeriodic = caps.ffbAirborne = true;
        }

        void Record(const char* n, float a = 0.f, float b = 0.f, float c = 0.f)
        {
            calls.push_back({ n, a, b, c });
        }

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

        Vendor      GetVendor()      const override { return Vendor::Logitech; }
        BackendKind GetKind()        const override { return BackendKind::Unknown; }
        const char* GetBackendName() const override { return "fake"; }

        bool Init() override  { return true; }
        bool Open() override  { open = true; return true; }
        void Close() override { open = false; Record("Close"); }
        bool IsOpen() const override { return open; }
        void Pump() override  { Record("Pump"); }

        const gwheel::wheel::Snapshot& CurrentSnapshot() const override { return snap; }
        const DeviceInfo&   GetDeviceInfo()   const override { return info; }
        const Capabilities& GetCapabilities() const override { return caps; }

        void PlayConstant(float m) override        { Record("PlayConstant", m); }
        void StopConstant() override               { Record("StopConstant"); }
        void PlaySpring(float c) override          { Record("PlaySpring", c); }
        void StopSpring() override                 { Record("StopSpring"); }
        void PlayDamper(float c) override          { Record("PlayDamper", c); }
        void StopDamper() override                 { Record("StopDamper"); }
        void PlayPeriodic(float m, int p) override { Record("PlayPeriodic", m, static_cast<float>(p)); }
        void StopPeriodic() override               { Record("StopPeriodic"); }
        void SetAirborne(bool on) override         { Record("SetAirborne", on ? 1.f : 0.f); }
        void StopAll() override                    { Record("StopAll"); }

        void TfFeedSample(float s) override        { tfSamples.push_back(s); }
        void TfClearStream() override              { Record("TfClearStream"); }
        void TfSetGain(float g) override           { Record("TfSetGain", g); }
        int  GetHapticRateHz() const override      { return 0; }

        void PlayLeds(float cur, float first, float red) override { Record("PlayLeds", cur, first, red); }
        void ClearLeds() override                  { Record("ClearLeds"); }
        void SetLedLevel(float l) override         { Record("SetLedLevel", l); }

        void SetOperatingRangeDeg(int d) override  { Record("SetOperatingRangeDeg", static_cast<float>(d)); }
        void SetGlobalGain(float g) override       { Record("SetGlobalGain", g); }

        float GetWheelAngleDeg() const override           { return 0.f; }
        float GetWheelAngularVelocityDeg() const override { return 0.f; }
        int   GetOperatingRangeDeg() const override       { return rangeDeg; }
        bool  HasFFB() const override                     { return ffb; }
    };
}
