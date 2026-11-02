# Unit tests for the gwheel plugin.
#
# The test executable contains every plugin source file except dllmain.cpp.
# Files whose internals are tested are #included by their test file (so the
# test can reach anonymous-namespace helpers and state); the rest are
# compiled as-is so the link is complete. Hardware, Windows input and the
# game stay out of the picture:
#   - Logitech SDK      -> support/fake_logitech_sdk.cpp (the .lib is not linked)
#   - wheel backend     -> support/fake_backend.h, installed into wheel.cpp
#   - SendInput         -> captured by test_input_bindings.cpp
#   - game memory       -> plain buffers; game-only paths are never called

set(GWHEEL_SRC "${CMAKE_SOURCE_DIR}/gwheel/src")

# Plugin sources with no test file of their own (or that only need to be
# linked in). Each source appears exactly once in the executable.
set(GWHEEL_LINKED_SOURCES
  ${GWHEEL_SRC}/device_table.cpp
  ${GWHEEL_SRC}/sources.cpp
  ${GWHEEL_SRC}/logi_common.cpp
  ${GWHEEL_SRC}/logging.cpp
  ${GWHEEL_SRC}/rtti_offsets.cpp
  ${GWHEEL_SRC}/tweakdb_physics.cpp
  ${GWHEEL_SRC}/trueforce_loader.cpp
  ${GWHEEL_SRC}/wheel_backend.cpp
  ${GWHEEL_SRC}/wheel_backend_logi_trueforce.cpp
)

set(GWHEEL_TEST_SOURCES
  test_main.cpp
  support/fake_logitech_sdk.cpp
  test_device_table.cpp
  test_sources.cpp
  test_logi_common.cpp
  test_logging.cpp
  test_axis_resolver.cpp      # includes axis_resolver.cpp
  test_config.cpp             # includes config.cpp
  test_input_bindings.cpp     # includes input_bindings.cpp, kbd_hook.cpp
  test_wheel.cpp              # includes wheel.cpp, wheel_tf_pipeline.cpp
  test_led.cpp                # includes led.cpp, audio_monitor.cpp
  test_legacy_backend.cpp     # includes wheel_backend_logi_legacy.cpp
  test_steering_speed.cpp     # includes steering_speed.cpp
  test_rtti.cpp               # includes rtti.cpp
  test_mod_settings_seed.cpp  # includes mod_settings_seed.cpp
  test_vehicle_hook.cpp       # includes vehicle_hook.cpp
  test_plugin.cpp             # includes plugin.cpp
)

add_executable(gwheel_tests ${GWHEEL_TEST_SOURCES} ${GWHEEL_LINKED_SOURCES})

target_include_directories(gwheel_tests
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/support
    ${GWHEEL_SRC}
    ${CMAKE_SOURCE_DIR}/gwheel/include
    ${CMAKE_SOURCE_DIR}/vendor/LogitechSDK/Include
)

target_link_libraries(gwheel_tests
  PRIVATE
    RED4ext::SDK
    dinput8
    dxguid
    ole32
    uuid
    mmdevapi
)

add_test(NAME gwheel_tests COMMAND gwheel_tests)

# Cross-checks between the redscript settings, the C++ action table and the
# button token format. Needs Python 3.9+; skipped when none is found.
find_package(Python3 3.9 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
  add_test(NAME reds_consistency
    COMMAND ${Python3_EXECUTABLE} -I -m unittest discover
            -s ${CMAKE_CURRENT_SOURCE_DIR}/python -p "test_*.py" -v)
endif()
