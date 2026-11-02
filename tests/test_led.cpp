#include "testfw.h"
#include "support/fake_backend.h"
#include "support/test_hooks.h"

#include "led.cpp"
#include "audio_monitor.cpp"

#include <vector>

using namespace gwheel;
using testsupport::FakeBackend;

namespace testsupport
{
    void SetAudioLevel(float level)
    {
        audio_monitor::g_level.store(level);
    }
}

namespace
{
    struct Fixture
    {
        FakeBackend backend;
        bool  dark = false;
        float rpm  = 0.f;
        Fixture()
        {
            testsupport::InstallBackend(&backend);
            testsupport::ResetConfigToDefaults();
            sources::SetInVehicle(true);
            sources::SetRadioActive(false);
            sources::ClearEngineProfile();
            sources::SetEngineRpmRaw(0.f);
            backend.calls.clear();
        }
        ~Fixture()
        {
            sources::SetInVehicle(false);
            sources::SetRadioActive(false);
            sources::ClearEngineProfile();
            sources::SetEngineRpmRaw(0.f);
            testsupport::SetAudioLevel(0.f);
            testsupport::ResetConfigToDefaults();
            testsupport::InstallBackend(nullptr);
        }
        void Tick() { led::Tick(dark, rpm); }
    };
}

// ---- LED controller ---------------------------------------------------------

TEST_CASE(led_dark_on_foot_and_cleared_once)
{
    Fixture fx;
    sources::SetInVehicle(false);
    fx.rpm = 3000.f;
    fx.Tick();
    fx.Tick();
    CHECK_EQ(fx.backend.Count("ClearLeds"), 1);
    CHECK(fx.dark);
    CHECK_EQ(fx.rpm, 0.f);                       // envelope restarts next mount
    CHECK_EQ(fx.backend.Count("PlayLeds"), 0);
}

TEST_CASE(led_disabled_setting_keeps_bar_dark)
{
    Fixture fx;
    config::SetLedEnabled(false);
    fx.backend.calls.clear();
    fx.Tick();
    fx.Tick();
    CHECK_EQ(fx.backend.Count("ClearLeds"), 1);
    CHECK_EQ(fx.backend.Count("PlayLeds"), 0);
    CHECK_EQ(fx.backend.Count("SetLedLevel"), 0);
}

TEST_CASE(led_rev_strip_tracks_rpm_against_gear_shift_point)
{
    Fixture fx;
    const float shifts[] = { 5500.f, 6000.f };
    sources::SetEngineProfile(1000.f, 7000.f, shifts, 2);
    sources::SetCurrentGear(2);
    sources::SetEngineRpmRaw(5000.f);

    fx.Tick();
    const auto* c = fx.backend.Last("PlayLeds");
    REQUIRE(c);
    CHECK_NEAR(c->a, 0.35f * 5000.f, 1e-3);      // smoothed RPM
    CHECK_NEAR(c->b, 1000.f + 0.6f * 5000.f, 1e-3);  // first LED at 60% to shift
    CHECK_NEAR(c->c, 6000.f, 1e-3);              // gear 2 shift point

    fx.Tick();
    CHECK_NEAR(fx.backend.Last("PlayLeds")->a, 1750.f + 0.35f * (5000.f - 1750.f), 1e-2);

    sources::SetCurrentGear(0);                  // neutral: redline is the shift point
    fx.Tick();
    CHECK_NEAR(fx.backend.Last("PlayLeds")->c, 7000.f, 1e-3);
    CHECK(!fx.dark);
}

TEST_CASE(led_visualizer_follows_audio_while_radio_plays)
{
    Fixture fx;
    sources::SetRadioActive(true);
    testsupport::SetAudioLevel(0.6f);
    fx.rpm = 2000.f;
    fx.Tick();
    const auto* c = fx.backend.Last("SetLedLevel");
    REQUIRE(c);
    CHECK_NEAR(c->a, 0.6f, 1e-6);
    CHECK_EQ(fx.rpm, 0.f);

    testsupport::SetAudioLevel(3.f);             // clamped to a full bar
    fx.Tick();
    CHECK_NEAR(fx.backend.Last("SetLedLevel")->a, 1.f, 1e-6);

    config::SetLedVisualizerWhileMusic(false);   // radio on, visualizer off: rev strip
    fx.backend.calls.clear();
    fx.Tick();
    CHECK_EQ(fx.backend.Count("SetLedLevel"), 0);
    CHECK_EQ(fx.backend.Count("PlayLeds"), 1);
}

// ---- Audio monitor DSP --------------------------------------------------------

namespace
{
    WAVEFORMATEX FloatStereo48k()
    {
        WAVEFORMATEX f{};
        f.wFormatTag      = WAVE_FORMAT_IEEE_FLOAT;
        f.nChannels       = 2;
        f.nSamplesPerSec  = 48000;
        f.wBitsPerSample  = 32;
        f.nBlockAlign     = 8;
        f.nAvgBytesPerSec = 48000 * 8;
        return f;
    }

    std::vector<float> StereoSine(float hz, float amp, int frames, int startFrame = 0)
    {
        std::vector<float> v(static_cast<size_t>(frames) * 2);
        for (int i = 0; i < frames; ++i)
        {
            const float s = amp * std::sin(2.f * 3.14159265f * hz * static_cast<float>(i + startFrame) / 48000.f);
            v[static_cast<size_t>(i) * 2]     = s;
            v[static_cast<size_t>(i) * 2 + 1] = s;
        }
        return v;
    }

    double SteadyMeanSquare(float hz, float amp)
    {
        using namespace audio_monitor;
        g_bassFilter.DesignBandpass(48000.f, 80.f, 0.7f);
        const WAVEFORMATEX fmt = FloatStereo48k();
        const auto warm = StereoSine(hz, amp, 9600);
        ChunkMeanSquare(reinterpret_cast<const BYTE*>(warm.data()), 9600, &fmt);
        const auto body = StereoSine(hz, amp, 48000, 9600);
        return ChunkMeanSquare(reinterpret_cast<const BYTE*>(body.data()), 48000, &fmt);
    }
}

TEST_CASE(audio_format_detection)
{
    using namespace audio_monitor;
    WAVEFORMATEX f = FloatStereo48k();
    CHECK(IsFloatFormat(&f));
    CHECK(!IsPcm16Format(&f));

    f.wFormatTag = WAVE_FORMAT_PCM;
    f.wBitsPerSample = 16;
    CHECK(!IsFloatFormat(&f));
    CHECK(IsPcm16Format(&f));
    f.wBitsPerSample = 24;
    CHECK(!IsPcm16Format(&f));

    WAVEFORMATEXTENSIBLE ext{};
    ext.Format.wFormatTag     = WAVE_FORMAT_EXTENSIBLE;
    ext.Format.cbSize         = 22;
    ext.Format.wBitsPerSample = 32;
    ext.SubFormat             = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
    CHECK(IsFloatFormat(&ext.Format));
    ext.SubFormat             = KSDATAFORMAT_SUBTYPE_PCM;
    ext.Format.wBitsPerSample = 16;
    CHECK(IsPcm16Format(&ext.Format));
    CHECK(!IsFloatFormat(&ext.Format));
    ext.Format.cbSize = 0;                       // too short to carry a SubFormat
    CHECK(!IsPcm16Format(&ext.Format));
}

TEST_CASE(audio_mean_square_edge_cases)
{
    using namespace audio_monitor;
    g_bassFilter.DesignBandpass(48000.f, 80.f, 0.7f);
    WAVEFORMATEX fmt = FloatStereo48k();
    const std::vector<float> silence(2000, 0.f);
    CHECK_EQ(ChunkMeanSquare(reinterpret_cast<const BYTE*>(silence.data()), 1000, &fmt), 0.0);
    CHECK_EQ(ChunkMeanSquare(reinterpret_cast<const BYTE*>(silence.data()), 0, &fmt), 0.0);
    fmt.nChannels = 0;
    CHECK_EQ(ChunkMeanSquare(reinterpret_cast<const BYTE*>(silence.data()), 1000, &fmt), 0.0);

    WAVEFORMATEX pcm24 = FloatStereo48k();
    pcm24.wFormatTag = WAVE_FORMAT_PCM;
    pcm24.wBitsPerSample = 24;
    const auto loud = StereoSine(80.f, 1.f, 1000);
    CHECK_EQ(ChunkMeanSquare(reinterpret_cast<const BYTE*>(loud.data()), 1000, &pcm24), 0.0);
}

TEST_CASE(audio_bass_band_passes_80hz_and_rejects_highs)
{
    const double pass = SteadyMeanSquare(80.f, 0.5f);
    CHECK_NEAR(pass, 0.5 * 0.5 * 0.5, 0.0125);   // unity gain at center: A^2/2
    const double high = SteadyMeanSquare(4000.f, 0.5f);
    CHECK(high < pass * 0.01);
    const double low = SteadyMeanSquare(5.f, 0.5f);
    CHECK(low < pass * 0.1);
}

TEST_CASE(audio_pcm16_matches_float_path)
{
    using namespace audio_monitor;
    const auto f = StereoSine(80.f, 0.5f, 4800);
    std::vector<int16_t> pcm(f.size());
    for (size_t i = 0; i < f.size(); ++i) pcm[i] = static_cast<int16_t>(std::lround(f[i] * 32767.f));

    WAVEFORMATEX fmtF = FloatStereo48k();
    WAVEFORMATEX fmtI = FloatStereo48k();
    fmtI.wFormatTag = WAVE_FORMAT_PCM;
    fmtI.wBitsPerSample = 16;

    g_bassFilter.DesignBandpass(48000.f, 80.f, 0.7f);
    const double a = ChunkMeanSquare(reinterpret_cast<const BYTE*>(f.data()), 4800, &fmtF);
    g_bassFilter.DesignBandpass(48000.f, 80.f, 0.7f);
    const double b = ChunkMeanSquare(reinterpret_cast<const BYTE*>(pcm.data()), 4800, &fmtI);
    CHECK_NEAR(a, b, a * 0.01);
}

TEST_CASE(audio_find_process_by_name)
{
    using namespace audio_monitor;
    CHECK_EQ(FindProcessIdByName(""), static_cast<DWORD>(0));
    CHECK_EQ(FindProcessIdByName("no-such-process-gwheel.exe"), static_cast<DWORD>(0));
    // Case-insensitive match on our own executable.
    CHECK(FindProcessIdByName("GWHEEL_TESTS.EXE") != 0);
}

TEST_CASE(audio_level_is_readable)
{
    testsupport::SetAudioLevel(0.25f);
    CHECK_NEAR(audio_monitor::CurrentLevel(), 0.25f, 1e-6);
    testsupport::SetAudioLevel(0.f);
}
