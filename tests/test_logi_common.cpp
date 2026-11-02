#include "testfw.h"

#include "logi_common.h"

using namespace gwheel;
using namespace gwheel::logi_common;

TEST_CASE(logi_normalize_steer_maps_full_range)
{
    CHECK_NEAR(NormalizeSteer(0), 0.f, 1e-6);
    CHECK_NEAR(NormalizeSteer(32767), 1.f, 1e-6);
    CHECK_NEAR(NormalizeSteer(-32767), -1.f, 1e-6);
    // -32768 would be just past -1; it is clamped.
    CHECK_EQ(NormalizeSteer(-32768), -1.f);
    CHECK_NEAR(NormalizeSteer(16384), 0.5f, 1e-4);
}

TEST_CASE(logi_normalize_pedal_is_inverted_zero_to_one)
{
    // DInput reports a released pedal at +32767 and a full press at -32768.
    CHECK_NEAR(NormalizePedal(32767), 0.f, 1e-6);
    CHECK_NEAR(NormalizePedal(-32768), 1.f, 1e-6);
    CHECK_NEAR(NormalizePedal(0), 0.5f, 1e-4);
    // Out-of-range input stays inside 0..1.
    CHECK_EQ(NormalizePedal(100000), 0.f);
    CHECK_EQ(NormalizePedal(-100000), 1.f);
}

TEST_CASE(logi_populate_snapshot_maps_axes_buttons_and_pov)
{
    DIJOYSTATE2 raw{};
    raw.lX  = 32767;     // full right
    raw.lY  = 32767;     // pedal released
    raw.lZ  = -32768;    // pedal full
    raw.lRx = 32767;
    raw.lRy = 32767;
    raw.lRz = 0;         // half
    raw.rglSlider[0] = -32768;
    raw.rglSlider[1] = 32767;
    raw.rgdwPOV[0] = 18000;
    raw.rgbButtons[0]  = 0x80;
    raw.rgbButtons[5]  = 0x80;
    raw.rgbButtons[31] = 0x80;
    raw.rgbButtons[32] = 0x80;  // beyond the 32-bit mask: ignored
    raw.rgbButtons[6]  = 0x7F;  // high bit clear: not pressed

    wheel::Snapshot s;
    PopulateSnapshotFromDInput(raw, s);

    CHECK_NEAR(s.axes[wheel::Axis_lX], 1.f, 1e-6);
    CHECK_NEAR(s.axes[wheel::Axis_lY], 0.f, 1e-6);
    CHECK_NEAR(s.axes[wheel::Axis_lZ], 1.f, 1e-6);
    CHECK_NEAR(s.axes[wheel::Axis_lRz], 0.5f, 1e-4);
    CHECK_NEAR(s.axes[wheel::Axis_slider0], 1.f, 1e-6);
    CHECK_NEAR(s.axes[wheel::Axis_slider1], 0.f, 1e-6);
    CHECK_EQ(s.pov, static_cast<uint16_t>(18000));
    CHECK_EQ(s.buttons, (1u << 0) | (1u << 5) | (1u << 31));
}

TEST_CASE(logi_populate_snapshot_centered_pov)
{
    DIJOYSTATE2 raw{};
    raw.rgdwPOV[0] = 0xFFFFFFFF;  // centered
    wheel::Snapshot s;
    PopulateSnapshotFromDInput(raw, s);
    CHECK_EQ(s.pov, static_cast<uint16_t>(0xFFFF));
    CHECK_EQ(s.buttons, 0u);
}
