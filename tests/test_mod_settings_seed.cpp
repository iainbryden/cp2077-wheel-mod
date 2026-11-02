#include "testfw.h"

#include "mod_settings_seed.cpp"

using namespace gwheel;
using namespace gwheel::mod_settings_seed;

// Run() itself probes the real DirectInput devices and writes the game's
// user.ini, so it is not called; its pieces are.

namespace
{
    DIDEVICEINSTANCEW Device(uint16_t vid, uint16_t pid, const wchar_t* name)
    {
        DIDEVICEINSTANCEW d{};
        d.dwSize = sizeof(d);
        d.guidProduct.Data1 = (static_cast<DWORD>(pid) << 16) | vid;
        wcsncpy_s(d.tszProductName, name, _TRUNCATE);
        return d;
    }

    std::wstring ReadIni(const std::wstring& path, const wchar_t* key)
    {
        wchar_t buf[16] = {};
        GetPrivateProfileStringW(L"GWheelSettings", key, L"?", buf, 16, path.c_str());
        return buf;
    }
}

TEST_CASE(seed_enum_accepts_logitech_wheels)
{
    DetectedWheel w;
    const auto dev = Device(0x046D, 0xC24F, L"G29 Driving Force Racing Wheel");
    CHECK_EQ(EnumDevicesCallback(&dev, &w), DIENUM_STOP);
    CHECK(w.found);
    CHECK_EQ(w.pid, static_cast<uint16_t>(0xC24F));
    CHECK_EQ(std::string(w.name), std::string("G29 Driving Force Racing Wheel"));
}

TEST_CASE(seed_enum_skips_other_devices)
{
    DetectedWheel w;
    const auto mouse = Device(0x046D, 0xC077, L"Logitech mouse");   // Logitech, not a wheel
    CHECK_EQ(EnumDevicesCallback(&mouse, &w), DIENUM_CONTINUE);
    const auto other = Device(0x044F, 0xB66E, L"Other wheel");      // not Logitech
    CHECK_EQ(EnumDevicesCallback(&other, &w), DIENUM_CONTINUE);
    CHECK(!w.found);
}

TEST_CASE(seed_user_ini_path_is_beside_the_plugin_folder)
{
    const auto p = ResolveUserIniPath();
    CHECK_EQ(p.filename().wstring(), std::wstring(L"user.ini"));
    CHECK_EQ(p.parent_path().filename().wstring(), std::wstring(L"mod_settings"));
}

TEST_CASE(seed_write_flag_writes_ini_values)
{
    wchar_t tmp[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, tmp);
    const std::wstring path = std::wstring(tmp) + L"gwheel_tests_user.ini";
    DeleteFileW(path.c_str());

    WriteFlag(path, L"hasRevLeds", true);
    WriteFlag(path, L"hasFfbHardware", false);
    CHECK(ReadIni(path, L"hasRevLeds") == L"1");
    CHECK(ReadIni(path, L"hasFfbHardware") == L"0");
    WriteFlag(path, L"hasRevLeds", false);
    CHECK(ReadIni(path, L"hasRevLeds") == L"0");
    CHECK(ReadIni(path, L"missing") == L"?");

    DeleteFileW(path.c_str());
}
