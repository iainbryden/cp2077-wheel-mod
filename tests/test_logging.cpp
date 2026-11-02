#include "testfw.h"

#include "logging.h"

#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <dinput.h>

#include <string>

using namespace gwheel;

TEST_CASE(logging_hresult_names_known_codes)
{
    CHECK_EQ(std::string(log::HresultName(S_OK)), std::string("S_OK"));
    CHECK_EQ(std::string(log::HresultName(DIERR_INPUTLOST)), std::string("DIERR_INPUTLOST"));
    CHECK_EQ(std::string(log::HresultName(DIERR_NOTACQUIRED)), std::string("DIERR_NOTACQUIRED"));
    CHECK(std::string(log::HresultName(E_ACCESSDENIED)).find("DIERR_OTHERAPPHASPRIO") != std::string::npos);
}

TEST_CASE(logging_hresult_formats_unknown_codes_as_hex)
{
    CHECK_EQ(std::string(log::HresultName(0x12345678)), std::string("0x12345678"));
    // E_UNEXPECTED is not in the named list.
    CHECK_EQ(std::string(log::HresultName(static_cast<long>(0x8000FFFFu))), std::string("0x8000FFFF"));
}

TEST_CASE(logging_debug_gate_toggles)
{
    log::SetDebugEnabled(true);
    CHECK(log::DebugEnabled());
    log::SetDebugEnabled(false);
    CHECK(!log::DebugEnabled());
}

TEST_CASE(logging_without_sdk_is_a_safe_no_op)
{
    // No RED4ext SDK is attached in tests: every log call must return
    // without touching a logger.
    log::Info("info");
    log::InfoF("%d %s", 1, "two");
    log::Warn("warn");
    log::ErrorF("%s", "error");
    log::SetDebugEnabled(true);
    log::DebugF("debug %d", 3);
    log::SetDebugEnabled(false);
    CHECK(true);
}
