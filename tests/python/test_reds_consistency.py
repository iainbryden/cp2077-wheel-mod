"""Cross-checks between the redscript side and the C++ plugin.

Neither compiler sees the other language, so a renamed action, a missing
native or a slider wider than the C++ clamp only shows up in game. These
tests read the sources and compare them.

Run: python -m unittest discover -s tests/python -v
"""

import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SETTINGS = ROOT / "gwheel_reds" / "gwheel_settings.reds"
NATIVES = ROOT / "gwheel_reds" / "gwheel_natives.reds"
REDS_DIR = ROOT / "gwheel_reds"
INPUT_BINDINGS = ROOT / "gwheel" / "src" / "input_bindings.cpp"
RTTI = ROOT / "gwheel" / "src" / "rtti.cpp"
CONFIG = ROOT / "gwheel" / "src" / "config.cpp"

AXIS_ACTIONS = {"SteerAxis", "ThrottleAxis", "BrakeAxis"}
POV_DIRECTIONS = {0, 9000, 18000, 27000}


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def button_enum(text: str) -> list[tuple[str, int]]:
    body = re.search(r"enum GWheelButton\s*\{(.*?)\}", text, re.S).group(1)
    return [(m.group(1), int(m.group(2))) for m in re.finditer(r"(\w+)\s*=\s*(\d+)", body)]


def button_tokens(text: str) -> dict[str, str]:
    body = re.search(r"func ButtonToken\(b: GWheelButton\) -> String \{(.*?)\n  \}", text, re.S).group(1)
    return dict(re.findall(r'Equals\(b, GWheelButton\.(\w+)\) \{ return "([^"]*)"; \}', body))


def cpp_action_names() -> list[str]:
    body = re.search(r"kActionNames = \{\{(.*?)\}\};", read(INPUT_BINDINGS), re.S).group(1)
    return re.findall(r'"(\w+)"', body)


def pushed_bindings(text: str) -> list[tuple[str, str]]:
    return re.findall(r'GWheel_SetActionBinding\("(\w+)",\s*([^;]+)\);', text)


def field_blocks(text: str) -> dict[str, list[str]]:
    """Map each `let name` field to the @runtimeProperty lines above it."""
    blocks: dict[str, list[str]] = {}
    pending: list[str] = []
    for line in text.splitlines():
        s = line.strip()
        if s.startswith("@runtimeProperty"):
            pending.append(s)
            continue
        m = re.match(r"let (\w+):", s)
        if m:
            blocks[m.group(1)] = pending
        pending = []
    return blocks


def prop(block: list[str], key: str) -> str | None:
    for line in block:
        m = re.match(r'@runtimeProperty\("ModSettings\.' + re.escape(key) + r'", "([^"]*)"\)', line)
        if m:
            return m.group(1)
    return None


class ButtonEnumTests(unittest.TestCase):
    def setUp(self):
        self.text = read(SETTINGS)
        self.enum = button_enum(self.text)
        self.tokens = button_tokens(self.text)

    def test_enum_is_dense_from_zero(self):
        # Mod Settings stores enum settings by position, so values must be
        # 0..N-1 in declaration order.
        self.assertEqual([v for _, v in self.enum], list(range(len(self.enum))))
        self.assertEqual(self.enum[0][0], "None")

    def test_every_button_has_exactly_one_valid_token(self):
        names = [n for n, _ in self.enum]
        self.assertNotIn("None", self.tokens)
        self.assertEqual(set(self.tokens), set(names) - {"None"})
        self.assertEqual(len(set(self.tokens.values())), len(self.tokens), "duplicate tokens")
        for name, token in self.tokens.items():
            m = re.fullmatch(r"wheel:(\d+)|wheel-pov:(\d+)", token)
            self.assertIsNotNone(m, f"{name}: bad token {token!r}")
            if m.group(1) is not None:
                # input_bindings::ParseToken accepts buttons 0..31 only.
                self.assertLessEqual(int(m.group(1)), 31, name)
            else:
                self.assertIn(int(m.group(2)), POV_DIRECTIONS, name)


class ActionBindingTests(unittest.TestCase):
    def setUp(self):
        self.text = read(SETTINGS)
        self.pushes = pushed_bindings(self.text)
        self.cpp_actions = cpp_action_names()

    def test_cpp_action_table_matches_enum_size(self):
        cpp = read(INPUT_BINDINGS)
        enum_body = re.search(r"enum Action : int32_t\s*\{(.*?)kActionCount", cpp, re.S).group(1)
        # A short kActionNames still compiles (missing entries become null),
        # so count both sides here.
        enum_names = re.findall(r"^\s*(\w+)\s*(?:=\s*\d+\s*)?,", enum_body, re.M)
        self.assertEqual(len(enum_names), len(self.cpp_actions))
        self.assertEqual(len(set(self.cpp_actions)), len(self.cpp_actions))

    def test_each_action_pushed_once(self):
        names = [n for n, _ in self.pushes]
        self.assertEqual(len(names), len(set(names)), "an action is pushed twice")

    def test_pushed_names_are_known_to_cpp(self):
        known = set(self.cpp_actions) | AXIS_ACTIONS
        for name, _ in self.pushes:
            self.assertIn(name, known, f"{name} is pushed but no C++ action handles it")

    def test_every_cpp_action_is_exposed_in_the_menu(self):
        pushed = {n for n, _ in self.pushes}
        missing = set(self.cpp_actions) - {"None"} - pushed
        self.assertFalse(missing, f"C++ actions with no setting: {sorted(missing)}")

    def test_button_pushes_use_their_own_dropdown(self):
        for name, expr in self.pushes:
            if name in AXIS_ACTIONS:
                self.assertEqual(expr.strip(), f"this.bind{name}")
            else:
                self.assertEqual(expr.strip(), f"GWheelSettings.ButtonToken(this.btn{name})")

    def test_button_dropdowns_label_every_value(self):
        blocks = field_blocks(self.text)
        enum_names = [n for n, _ in button_enum(self.text)]
        for name, _ in self.pushes:
            if name in AXIS_ACTIONS:
                continue
            block = blocks.get(f"btn{name}")
            self.assertIsNotNone(block, f"btn{name} field missing")
            for value in enum_names:
                self.assertIsNotNone(prop(block, f"displayValues.{value}"), f"btn{name}: no label for {value}")
            self.assertIsNotNone(prop(block, "category"), f"btn{name}: no category")
            self.assertIsNotNone(prop(block, "displayName"), f"btn{name}: no display name")

    def test_axis_defaults_are_wheel_axis_tokens(self):
        for axis in sorted(AXIS_ACTIONS):
            m = re.search(rf'let bind{axis}: String = "([^"]*)";', self.text)
            self.assertIsNotNone(m, axis)
            self.assertRegex(m.group(1), r"^wheel-axis:(lX|lY|lZ|lRx|lRy|lRz|slider0|slider1)$")


class NativeTests(unittest.TestCase):
    def test_declared_natives_are_registered_and_vice_versa(self):
        declared = set(re.findall(r"native func (GWheel_\w+)\(", read(NATIVES)))
        registered = re.findall(r'RegisterGlobal\(rtti,\s*"(GWheel_\w+)"', read(RTTI))
        self.assertEqual(len(registered), len(set(registered)), "a native is registered twice")
        self.assertEqual(declared, set(registered))

    def test_scripts_only_call_declared_natives(self):
        declared = set(re.findall(r"native func (GWheel_\w+)\(", read(NATIVES)))
        for path in REDS_DIR.glob("*.reds"):
            for call in set(re.findall(r"\b(GWheel_\w+)\(", read(path))):
                self.assertIn(call, declared, f"{path.name} calls undeclared {call}")


class SliderRangeTests(unittest.TestCase):
    # Mod Settings slider -> C++ setter that receives it.
    INT_SETTERS = {
        "steerDeadzonePct": "SetSteerDeadzonePct",
        "steerGainPct": "SetSteerGainPct",
        "steerSpeedPct": "SetSteerSpeedPct",
        "ffbTorquePct": "SetFfbTorquePct",
        "yawFeedbackPct": "SetYawFeedbackPct",
        "activeTorqueStrengthPct": "SetActiveTorqueStrengthPct",
    }

    def setUp(self):
        self.blocks = field_blocks(read(SETTINGS))
        cpp = read(CONFIG)
        self.clamps = {
            m.group(1): (float(m.group(2)), float(m.group(3)))
            for m in re.finditer(r"void (Set\w+)\((?:int32_t|float) v\)\s*\{[^\n]*std::clamp\(v, (-?[\d.]+)f?, (-?[\d.]+)f?\)", cpp)
        }

    def slider(self, field):
        block = self.blocks[field]
        return float(prop(block, "min")), float(prop(block, "max"))

    def test_sliders_match_cpp_clamps(self):
        for field, setter in self.INT_SETTERS.items():
            with self.subTest(field=field):
                self.assertIn(setter, self.clamps)
                self.assertEqual(self.slider(field), self.clamps[setter])

    def test_stationary_threshold_slider_inside_cpp_range(self):
        lo, hi = self.slider("stationaryThresholdMps")
        clo, chi = self.clamps["SetStationaryThresholdMps"]
        self.assertGreaterEqual(lo, clo)
        self.assertLessEqual(hi, chi)

    def test_wheel_range_slider_matches_cpp(self):
        # 0 = auto; the C++ setter maps 1..39 up to 40.
        self.assertEqual(self.slider("ffbWheelRangeDeg"), (0.0, 1080.0))
        self.assertIn("std::clamp(v, 40, 1080)", read(CONFIG))

    def test_slider_defaults_inside_their_range(self):
        text = read(SETTINGS)
        for field in list(self.INT_SETTERS) + ["ffbWheelRangeDeg", "stationaryThresholdMps"]:
            with self.subTest(field=field):
                m = re.search(rf"let {field}: \w+ = (-?[\d.]+);", text)
                self.assertIsNotNone(m)
                lo, hi = self.slider(field)
                self.assertTrue(lo <= float(m.group(1)) <= hi)


if __name__ == "__main__":
    unittest.main()
