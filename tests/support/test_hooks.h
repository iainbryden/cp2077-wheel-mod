#pragma once

// Test-only entry points into plugin internals. Each one is defined in the
// test file that #includes the matching production .cpp, so it can reach
// that file's anonymous-namespace state.

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace testsupport
{
    class FakeBackend;

    // test_wheel.cpp: replaces wheel.cpp's active backend (nullptr removes
    // it). The backend is owned by the caller and must outlive its use.
    void InstallBackend(FakeBackend* backend);

    // test_wheel.cpp: clears the FFB edge state (last-sent percentages,
    // airborne latch, surface envelope, jolt) between test cases.
    void ResetFfbState();

    // test_led.cpp: sets the audio level the LED visualizer reads.
    void SetAudioLevel(float level);

    // test_input_bindings.cpp: synthetic input events captured instead of
    // being sent to Windows.
    struct SentInput
    {
        bool     keyboard = false;  // false = mouse
        uint16_t vk       = 0;
        bool     keyUp    = false;
        uint32_t mouseFlags = 0;
        int32_t  mouseData  = 0;
        uintptr_t extraInfo = 0;
    };
    std::vector<SentInput>& SentInputs();

    // test_config.cpp: path of the config.json the plugin reads and writes
    // in the test process (next to the test executable).
    std::filesystem::path ConfigPathForTests();

    // Restores every config field to its default (persists to the test
    // config.json, which is deleted at the end of the run).
    void ResetConfigToDefaults();
}
