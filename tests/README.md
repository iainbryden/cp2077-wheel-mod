# Unit tests

Two suites, both run by CTest:

- `gwheel_tests`: C++ tests for the plugin (163 cases).
- `reds_consistency`: Python checks that the redscript settings, the natives
  and the C++ tables agree (15 cases).

## Running

```powershell
cmake -S . -B build/tests -G "Visual Studio 16 2019" -A x64 -DGWHEEL_BUILD_TESTS=ON
cmake --build build/tests --config Release --target gwheel_tests
ctest --test-dir build/tests -C Release --output-on-failure
```

`GWHEEL_BUILD_TESTS` is off by default, so the normal plugin build is
unchanged. `build/tests/tests/Release/gwheel_tests.exe <text>` runs only the
cases whose name contains `<text>`. The Python suite needs Python 3.9 or later
on `PATH` (or pass `-DPython3_EXECUTABLE=...`), and can also be run on its own:

```powershell
python -m unittest discover -s tests/python -v
```

## How it works

The test executable contains every plugin source except `dllmain.cpp`. A test
file `#include`s the `.cpp` it tests, so it can reach that file's internal
helpers and state without changing plugin code. Nothing outside the process
is touched:

| Real dependency | Replaced by |
|---|---|
| Logitech SDK (`LogitechSteeringWheelLib.lib`) | `support/fake_logitech_sdk.cpp`; the .lib is not linked |
| Wheel backend in `wheel.cpp` | `support/fake_backend.h`, records every effect and LED call |
| `SendInput` | captured in `test_input_bindings.cpp`; no key or mouse event is sent |
| Game vehicle object | a zeroed buffer |
| RED4ext logger | absent; logging is a no-op |

Files the tests write (`config.json`, `steering_speed.applying` beside the
executable, an `.ini` in `%TEMP%`) are deleted when the tests finish.

## Coverage

Covered: device table, input sources, DirectInput normalisation, axis bindings
and steering curves, config parsing / saving / setters, button bindings
(parsing, hold / tap / mouse dispatch, on-foot suppression, capture mode), the
G HUB keyboard filter, the force-feedback pipeline (forces, range scaling,
reverse, airborne, sliding, bikes, jolts, road surface, resend pacing, pause
watchdog), the TrueForce signal mix, LED rev strip and visualizer, the audio
bass filter, the legacy Logitech backend, the Mod Settings capability flags
and `user.ini` seeding, the vehicle steer / pedal write, the pump frame and
timer, and the steering-speed crash safeguards.

Not covered, because it only exists inside the running game or with the
hardware attached: the TweakDB scan and rewrite in `steering_speed.cpp`,
`tweakdb_physics.cpp` and `rtti_offsets.cpp` (game RTTI and memory), the
script natives' argument decoding in `rtti.cpp`, hook installation and the
FFB physics read in `vehicle_hook.cpp`, the TrueForce SDK backend and loader,
WASAPI capture in `audio_monitor.cpp`, DirectInput probing, and plugin
load / unload. Those still need in-game testing.

Some cases document current behaviour rather than ideal behaviour; their
comments say so (for example, config.json values are not range-checked at
load time).
