// In-game settings page for the gwheel mod.
//
// Single GWheelSettings class. Categories (`ModSettings.category` +
// `ModSettings.category.order`) provide visual grouping on the scrolling
// settings panel.
//
// Capability auto-detection (how the section-hide actually works):
//
//   1. Three hidden Bool fields (hasFfbHardware, hasRevLeds, hasRightCluster)
//      carry the runtime-property `ModSettings.hidden = "true"`. With our
//      patched mod_settings build they are excluded from the UI list but
//      still registered as variables.
//
//   2. Other settings reference these fields via `ModSettings.dependency`.
//      mod_settings hides the dependent setting whenever the dependency
//      target's stored value is not "true".
//
//   3. mod_settings reads its stored values from
//      `red4ext/plugins/mod_settings/user.ini` once at process start. So to
//      drive the three capability flags from real hardware state we have to
//      write that file BEFORE mod_settings reads it. That happens in
//      gwheel.dll's RED4ext OnLoad — see gwheel/src/mod_settings_seed.cpp.
//      Plugin OnLoads complete before script-data processing, so the
//      ordering is safe regardless of plugin load order.
//
//   4. Important consequence: the UI reflects the wheel state captured at
//      the moment of process start. Plug or unplug the wheel = restart the
//      game once for the section list to catch up.
//
// Phase 7 binding model: every player-facing input is a String holding a
// `|`-delimited list of tagged tokens emitted by the InputBinding primitive
// (e.g. "wheel-axis:lY", "wheel:5|keyboard:KEY_H"). Empty string = unbound.
// The C++ side resolves axes via gwheel::axis_resolver against the action
// name; buttons reach C++ via the same SetActionBinding native and will
// drive dispatch once the button-side action-keyed dispatcher lands.
//
// Tabula-rasa: every default is empty. New users bind from scratch via the
// Mod Settings UI; the in-vehicle banner (gwheel_mount.reds, future phase)
// nags them when SteerAxis/ThrottleAxis/BrakeAxis are unbound.

// Per Mod Settings memory rule: enum values persist by DROPDOWN POSITION,
// not by declared integer value. Never reorder, never leave gaps. New
// curves go at the end.
enum GWheelResponseCurve {
  Default = 0,
  Subdued = 1,
  Sharp   = 2,
}

// Wheel buttons for the in-menu binding dropdowns. Persisted by dropdown
// POSITION like every Mod Settings enum: append only, never reorder.
enum GWheelButton {
  None = 0,
  Cross = 1,
  Square = 2,
  Circle = 3,
  Triangle = 4,
  PaddleRight = 5,
  PaddleLeft = 6,
  R2 = 7,
  L2 = 8,
  Share = 9,
  Options = 10,
  R3 = 11,
  L3 = 12,
  Plus = 13,
  Minus = 14,
  DialRight = 15,
  DialLeft = 16,
  DialPress = 17,
  PS = 18,
  DpadUp = 19,
  DpadRight = 20,
  DpadDown = 21,
  DpadLeft = 22,
}

public class GWheelSettings extends IScriptable {

  // ---- Hidden capability flags (auto-set in OnGameAttached) --------------

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.hidden", "true")
  let hasFfbHardware: Bool = true;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.hidden", "true")
  let hasRevLeds: Bool = true;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.hidden", "true")
  let hasRightCluster: Bool = true;

  // ---- Wheel input --------------------------------------------------------

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Wheel input")
  @runtimeProperty("ModSettings.category.order", "100")
  @runtimeProperty("ModSettings.displayName", "Enable wheel input")
  let inputEnabled: Bool = true;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Wheel input")
  @runtimeProperty("ModSettings.category.order", "100")
  @runtimeProperty("ModSettings.displayName", "Steering deadzone (%)")
  @runtimeProperty("ModSettings.description", "Filters small wheel-center jitter so the car doesn't drift when your hands are off the wheel. The 0 percent setting passes raw axis values through. 2-3 percent is typical for Logitech wheels; 5-10 percent if the wheel reports a large idle offset.")
  @runtimeProperty("ModSettings.min", "0")
  @runtimeProperty("ModSettings.max", "15")
  @runtimeProperty("ModSettings.step", "1")
  let steerDeadzonePct: Int32 = 2;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Wheel input")
  @runtimeProperty("ModSettings.category.order", "100")
  @runtimeProperty("ModSettings.displayName", "Steering sensitivity (%)")
  @runtimeProperty("ModSettings.description", "How much the car steers per degree of wheel rotation. 100 means full wheel lock gives full game lock. 200 reaches full game lock at half the wheel rotation. Raise it if you have to turn the wheel too far; lower it if steering is twitchy.")
  @runtimeProperty("ModSettings.min", "50")
  @runtimeProperty("ModSettings.max", "400")
  @runtimeProperty("ModSettings.step", "10")
  let steerGainPct: Int32 = 300;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Wheel input")
  @runtimeProperty("ModSettings.category.order", "100")
  @runtimeProperty("ModSettings.displayName", "Steering speed (%)")
  @runtimeProperty("ModSettings.description", "How fast the car's front wheels follow the steering wheel. The game limits this per vehicle (about 100 degrees per second), which feels like lag on a wheel. 100 keeps the game's speed; 300 is three times faster. Applies to vehicles spawned after the change: re-summon your car or reload a save.")
  @runtimeProperty("ModSettings.min", "100")
  @runtimeProperty("ModSettings.max", "1000")
  @runtimeProperty("ModSettings.step", "25")
  let steerSpeedPct: Int32 = 300;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Wheel input")
  @runtimeProperty("ModSettings.category.order", "100")
  @runtimeProperty("ModSettings.displayName", "Response curve")
  @runtimeProperty("ModSettings.description", "Reshapes how wheel rotation maps to steering. Default is linear. Subdued softens response near center for relaxed cruising. Sharp pulls more response near center for tight handling at the cost of twitchiness.")
  @runtimeProperty("ModSettings.displayValues.Default", "Default")
  @runtimeProperty("ModSettings.displayValues.Subdued", "Subdued")
  @runtimeProperty("ModSettings.displayValues.Sharp",   "Sharp")
  let responseCurve: GWheelResponseCurve = GWheelResponseCurve.Default;

  // ---- Force feedback -----------------------------------------------------

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Force feedback")
  @runtimeProperty("ModSettings.category.order", "200")
  @runtimeProperty("ModSettings.displayName", "Enable force feedback")
  @runtimeProperty("ModSettings.dependency", "hasFfbHardware")
  let ffbEnabled: Bool = true;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Force feedback")
  @runtimeProperty("ModSettings.category.order", "200")
  @runtimeProperty("ModSettings.displayName", "FFB strength (%)")
  @runtimeProperty("ModSettings.min", "0")
  @runtimeProperty("ModSettings.max", "100")
  @runtimeProperty("ModSettings.step", "5")
  @runtimeProperty("ModSettings.dependency", "hasFfbHardware")
  let ffbTorquePct: Int32 = 40;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Force feedback")
  @runtimeProperty("ModSettings.category.order", "200")
  @runtimeProperty("ModSettings.displayName", "Wheel rotation (degrees)")
  @runtimeProperty("ModSettings.description", "Set this to the Operating Range in your G HUB profile, so force feedback is scaled correctly for how far your wheel turns. 0 asks G HUB, but current G HUB versions don't answer, and the mod then assumes 540.")
  @runtimeProperty("ModSettings.min", "0")
  @runtimeProperty("ModSettings.max", "1080")
  @runtimeProperty("ModSettings.step", "10")
  @runtimeProperty("ModSettings.dependency", "hasFfbHardware")
  let ffbWheelRangeDeg: Int32 = 180;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Force feedback")
  @runtimeProperty("ModSettings.category.order", "200")
  @runtimeProperty("ModSettings.displayName", "Cornering feedback (%)")
  @runtimeProperty("ModSettings.description", "Adds spring stiffness while the car is rotating.")
  @runtimeProperty("ModSettings.min", "0")
  @runtimeProperty("ModSettings.max", "100")
  @runtimeProperty("ModSettings.step", "5")
  @runtimeProperty("ModSettings.dependency", "hasFfbHardware")
  let yawFeedbackPct: Int32 = 50;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Force feedback")
  @runtimeProperty("ModSettings.category.order", "200")
  @runtimeProperty("ModSettings.displayName", "Active torque (%)")
  @runtimeProperty("ModSettings.description", "How hard the wheel pushes toward center, scaled by speed and steering angle.")
  @runtimeProperty("ModSettings.min", "0")
  @runtimeProperty("ModSettings.max", "100")
  @runtimeProperty("ModSettings.step", "5")
  @runtimeProperty("ModSettings.dependency", "hasFfbHardware")
  let activeTorqueStrengthPct: Int32 = 100;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Force feedback")
  @runtimeProperty("ModSettings.category.order", "200")
  @runtimeProperty("ModSettings.displayName", "Stationary threshold (m/s)")
  @runtimeProperty("ModSettings.description", "Below this speed the wheel has no centering force.")
  @runtimeProperty("ModSettings.min", "0.0")
  @runtimeProperty("ModSettings.max", "5.0")
  @runtimeProperty("ModSettings.step", "0.1")
  @runtimeProperty("ModSettings.dependency", "hasFfbHardware")
  let stationaryThresholdMps: Float = 0.5;

  // Conceptually an input-direction toggle (turning the wheel right makes
  // the car turn left); FFB pipeline follows automatically as a side
  // effect of how the math is wired. Lives in Wheel input rather than
  // Force feedback so the section reflects the user's mental model.
  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Wheel input")
  @runtimeProperty("ModSettings.category.order", "100")
  @runtimeProperty("ModSettings.displayName", "Invert steering")
  @runtimeProperty("ModSettings.description", "Reverse the wheel's steering direction so turning the wheel right makes the car turn left. Useful for inverted-wheel mount setups. Restoring forces follow automatically so the wheel still self-centers correctly in the mirrored world.")
  let ffbInvertDirection: Bool = false;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Force feedback")
  @runtimeProperty("ModSettings.category.order", "200")
  @runtimeProperty("ModSettings.displayName", "Invert FFB direction")
  @runtimeProperty("ModSettings.description", "If your wheel snaps to full lock instead of centering when you start driving, enable this. Common on Logitech G HUB wheels (verified on G923) where the SDK's constant-force direction is reported opposite to documentation.")
  @runtimeProperty("ModSettings.dependency", "hasFfbHardware")
  let ffbOutputSignFlip: Bool = false;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Force feedback")
  @runtimeProperty("ModSettings.category.order", "200")
  @runtimeProperty("ModSettings.displayName", "Speed-sensitive stiffness")
  @runtimeProperty("ModSettings.description", "On: wheel firms up at speed (sportscar-style steering assist). Off: pure tire-force feedback only, like BeamNG. The wheel goes light at rest and feels more raw at speed.")
  @runtimeProperty("ModSettings.dependency", "hasFfbHardware")
  let ffbUseSpring: Bool = true;

  // ---- Rev-strip LEDs -----------------------------------------------------

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Rev-strip LEDs")
  @runtimeProperty("ModSettings.category.order", "300")
  @runtimeProperty("ModSettings.displayName", "Enable rev-strip LEDs")
  @runtimeProperty("ModSettings.dependency", "hasRevLeds")
  let ledEnabled: Bool = true;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Rev-strip LEDs")
  @runtimeProperty("ModSettings.category.order", "300")
  @runtimeProperty("ModSettings.displayName", "Rev strip as visualizer while music is playing")
  @runtimeProperty("ModSettings.dependency", "hasRevLeds")
  let ledVisualizerWhileMusic: Bool = true;

  // ---- Steering & pedals (axis bindings) ---------------------------------
  //
  // The three axes the car cannot drive without. Defaults intentionally
  // empty (tabula rasa); first-run UX prompts the user to bind them.

  let bindSteerAxis: String = "wheel-axis:lX";

  let bindThrottleAxis: String = "wheel-axis:lY";

  let bindBrakeAxis: String = "wheel-axis:lRz";

  // ---- Driving ------------------------------------------------------------

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Buttons: driving")
  @runtimeProperty("ModSettings.category.order", "400")
  @runtimeProperty("ModSettings.displayName", "Horn")
  @runtimeProperty("ModSettings.description", "Held while the button is held.")
  @runtimeProperty("ModSettings.displayValues.None", "Not bound")
  @runtimeProperty("ModSettings.displayValues.Cross", "Cross (X)")
  @runtimeProperty("ModSettings.displayValues.Square", "Square")
  @runtimeProperty("ModSettings.displayValues.Circle", "Circle")
  @runtimeProperty("ModSettings.displayValues.Triangle", "Triangle")
  @runtimeProperty("ModSettings.displayValues.PaddleRight", "Right paddle")
  @runtimeProperty("ModSettings.displayValues.PaddleLeft", "Left paddle")
  @runtimeProperty("ModSettings.displayValues.R2", "R2")
  @runtimeProperty("ModSettings.displayValues.L2", "L2")
  @runtimeProperty("ModSettings.displayValues.Share", "Share")
  @runtimeProperty("ModSettings.displayValues.Options", "Options")
  @runtimeProperty("ModSettings.displayValues.R3", "R3")
  @runtimeProperty("ModSettings.displayValues.L3", "L3")
  @runtimeProperty("ModSettings.displayValues.Plus", "Plus (+)")
  @runtimeProperty("ModSettings.displayValues.Minus", "Minus (-)")
  @runtimeProperty("ModSettings.displayValues.DialRight", "Red dial right")
  @runtimeProperty("ModSettings.displayValues.DialLeft", "Red dial left")
  @runtimeProperty("ModSettings.displayValues.DialPress", "Red dial press")
  @runtimeProperty("ModSettings.displayValues.PS", "PS button")
  @runtimeProperty("ModSettings.displayValues.DpadUp", "D-pad up")
  @runtimeProperty("ModSettings.displayValues.DpadRight", "D-pad right")
  @runtimeProperty("ModSettings.displayValues.DpadDown", "D-pad down")
  @runtimeProperty("ModSettings.displayValues.DpadLeft", "D-pad left")
  let btnHorn: GWheelButton = GWheelButton.None;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Buttons: driving")
  @runtimeProperty("ModSettings.category.order", "400")
  @runtimeProperty("ModSettings.displayName", "Headlights")
  @runtimeProperty("ModSettings.description", "Cycles the lights (V).")
  @runtimeProperty("ModSettings.displayValues.None", "Not bound")
  @runtimeProperty("ModSettings.displayValues.Cross", "Cross (X)")
  @runtimeProperty("ModSettings.displayValues.Square", "Square")
  @runtimeProperty("ModSettings.displayValues.Circle", "Circle")
  @runtimeProperty("ModSettings.displayValues.Triangle", "Triangle")
  @runtimeProperty("ModSettings.displayValues.PaddleRight", "Right paddle")
  @runtimeProperty("ModSettings.displayValues.PaddleLeft", "Left paddle")
  @runtimeProperty("ModSettings.displayValues.R2", "R2")
  @runtimeProperty("ModSettings.displayValues.L2", "L2")
  @runtimeProperty("ModSettings.displayValues.Share", "Share")
  @runtimeProperty("ModSettings.displayValues.Options", "Options")
  @runtimeProperty("ModSettings.displayValues.R3", "R3")
  @runtimeProperty("ModSettings.displayValues.L3", "L3")
  @runtimeProperty("ModSettings.displayValues.Plus", "Plus (+)")
  @runtimeProperty("ModSettings.displayValues.Minus", "Minus (-)")
  @runtimeProperty("ModSettings.displayValues.DialRight", "Red dial right")
  @runtimeProperty("ModSettings.displayValues.DialLeft", "Red dial left")
  @runtimeProperty("ModSettings.displayValues.DialPress", "Red dial press")
  @runtimeProperty("ModSettings.displayValues.PS", "PS button")
  @runtimeProperty("ModSettings.displayValues.DpadUp", "D-pad up")
  @runtimeProperty("ModSettings.displayValues.DpadRight", "D-pad right")
  @runtimeProperty("ModSettings.displayValues.DpadDown", "D-pad down")
  @runtimeProperty("ModSettings.displayValues.DpadLeft", "D-pad left")
  let btnHeadlights: GWheelButton = GWheelButton.None;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Buttons: driving")
  @runtimeProperty("ModSettings.category.order", "400")
  @runtimeProperty("ModSettings.displayName", "Handbrake")
  @runtimeProperty("ModSettings.description", "Held while the button is held.")
  @runtimeProperty("ModSettings.displayValues.None", "Not bound")
  @runtimeProperty("ModSettings.displayValues.Cross", "Cross (X)")
  @runtimeProperty("ModSettings.displayValues.Square", "Square")
  @runtimeProperty("ModSettings.displayValues.Circle", "Circle")
  @runtimeProperty("ModSettings.displayValues.Triangle", "Triangle")
  @runtimeProperty("ModSettings.displayValues.PaddleRight", "Right paddle")
  @runtimeProperty("ModSettings.displayValues.PaddleLeft", "Left paddle")
  @runtimeProperty("ModSettings.displayValues.R2", "R2")
  @runtimeProperty("ModSettings.displayValues.L2", "L2")
  @runtimeProperty("ModSettings.displayValues.Share", "Share")
  @runtimeProperty("ModSettings.displayValues.Options", "Options")
  @runtimeProperty("ModSettings.displayValues.R3", "R3")
  @runtimeProperty("ModSettings.displayValues.L3", "L3")
  @runtimeProperty("ModSettings.displayValues.Plus", "Plus (+)")
  @runtimeProperty("ModSettings.displayValues.Minus", "Minus (-)")
  @runtimeProperty("ModSettings.displayValues.DialRight", "Red dial right")
  @runtimeProperty("ModSettings.displayValues.DialLeft", "Red dial left")
  @runtimeProperty("ModSettings.displayValues.DialPress", "Red dial press")
  @runtimeProperty("ModSettings.displayValues.PS", "PS button")
  @runtimeProperty("ModSettings.displayValues.DpadUp", "D-pad up")
  @runtimeProperty("ModSettings.displayValues.DpadRight", "D-pad right")
  @runtimeProperty("ModSettings.displayValues.DpadDown", "D-pad down")
  @runtimeProperty("ModSettings.displayValues.DpadLeft", "D-pad left")
  let btnHandbrake: GWheelButton = GWheelButton.None;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Buttons: driving")
  @runtimeProperty("ModSettings.category.order", "400")
  @runtimeProperty("ModSettings.displayName", "Autodrive")
  @runtimeProperty("ModSettings.description", "Hold to engage (G).")
  @runtimeProperty("ModSettings.displayValues.None", "Not bound")
  @runtimeProperty("ModSettings.displayValues.Cross", "Cross (X)")
  @runtimeProperty("ModSettings.displayValues.Square", "Square")
  @runtimeProperty("ModSettings.displayValues.Circle", "Circle")
  @runtimeProperty("ModSettings.displayValues.Triangle", "Triangle")
  @runtimeProperty("ModSettings.displayValues.PaddleRight", "Right paddle")
  @runtimeProperty("ModSettings.displayValues.PaddleLeft", "Left paddle")
  @runtimeProperty("ModSettings.displayValues.R2", "R2")
  @runtimeProperty("ModSettings.displayValues.L2", "L2")
  @runtimeProperty("ModSettings.displayValues.Share", "Share")
  @runtimeProperty("ModSettings.displayValues.Options", "Options")
  @runtimeProperty("ModSettings.displayValues.R3", "R3")
  @runtimeProperty("ModSettings.displayValues.L3", "L3")
  @runtimeProperty("ModSettings.displayValues.Plus", "Plus (+)")
  @runtimeProperty("ModSettings.displayValues.Minus", "Minus (-)")
  @runtimeProperty("ModSettings.displayValues.DialRight", "Red dial right")
  @runtimeProperty("ModSettings.displayValues.DialLeft", "Red dial left")
  @runtimeProperty("ModSettings.displayValues.DialPress", "Red dial press")
  @runtimeProperty("ModSettings.displayValues.PS", "PS button")
  @runtimeProperty("ModSettings.displayValues.DpadUp", "D-pad up")
  @runtimeProperty("ModSettings.displayValues.DpadRight", "D-pad right")
  @runtimeProperty("ModSettings.displayValues.DpadDown", "D-pad down")
  @runtimeProperty("ModSettings.displayValues.DpadLeft", "D-pad left")
  let btnAutodrive: GWheelButton = GWheelButton.None;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Buttons: driving")
  @runtimeProperty("ModSettings.category.order", "400")
  @runtimeProperty("ModSettings.displayName", "Exit vehicle")
  @runtimeProperty("ModSettings.description", "Hold to get out (F).")
  @runtimeProperty("ModSettings.displayValues.None", "Not bound")
  @runtimeProperty("ModSettings.displayValues.Cross", "Cross (X)")
  @runtimeProperty("ModSettings.displayValues.Square", "Square")
  @runtimeProperty("ModSettings.displayValues.Circle", "Circle")
  @runtimeProperty("ModSettings.displayValues.Triangle", "Triangle")
  @runtimeProperty("ModSettings.displayValues.PaddleRight", "Right paddle")
  @runtimeProperty("ModSettings.displayValues.PaddleLeft", "Left paddle")
  @runtimeProperty("ModSettings.displayValues.R2", "R2")
  @runtimeProperty("ModSettings.displayValues.L2", "L2")
  @runtimeProperty("ModSettings.displayValues.Share", "Share")
  @runtimeProperty("ModSettings.displayValues.Options", "Options")
  @runtimeProperty("ModSettings.displayValues.R3", "R3")
  @runtimeProperty("ModSettings.displayValues.L3", "L3")
  @runtimeProperty("ModSettings.displayValues.Plus", "Plus (+)")
  @runtimeProperty("ModSettings.displayValues.Minus", "Minus (-)")
  @runtimeProperty("ModSettings.displayValues.DialRight", "Red dial right")
  @runtimeProperty("ModSettings.displayValues.DialLeft", "Red dial left")
  @runtimeProperty("ModSettings.displayValues.DialPress", "Red dial press")
  @runtimeProperty("ModSettings.displayValues.PS", "PS button")
  @runtimeProperty("ModSettings.displayValues.DpadUp", "D-pad up")
  @runtimeProperty("ModSettings.displayValues.DpadRight", "D-pad right")
  @runtimeProperty("ModSettings.displayValues.DpadDown", "D-pad down")
  @runtimeProperty("ModSettings.displayValues.DpadLeft", "D-pad left")
  let btnExitVehicle: GWheelButton = GWheelButton.None;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Buttons: driving")
  @runtimeProperty("ModSettings.category.order", "400")
  @runtimeProperty("ModSettings.displayName", "Call vehicle")
  @runtimeProperty("ModSettings.description", "Summons your car (V).")
  @runtimeProperty("ModSettings.displayValues.None", "Not bound")
  @runtimeProperty("ModSettings.displayValues.Cross", "Cross (X)")
  @runtimeProperty("ModSettings.displayValues.Square", "Square")
  @runtimeProperty("ModSettings.displayValues.Circle", "Circle")
  @runtimeProperty("ModSettings.displayValues.Triangle", "Triangle")
  @runtimeProperty("ModSettings.displayValues.PaddleRight", "Right paddle")
  @runtimeProperty("ModSettings.displayValues.PaddleLeft", "Left paddle")
  @runtimeProperty("ModSettings.displayValues.R2", "R2")
  @runtimeProperty("ModSettings.displayValues.L2", "L2")
  @runtimeProperty("ModSettings.displayValues.Share", "Share")
  @runtimeProperty("ModSettings.displayValues.Options", "Options")
  @runtimeProperty("ModSettings.displayValues.R3", "R3")
  @runtimeProperty("ModSettings.displayValues.L3", "L3")
  @runtimeProperty("ModSettings.displayValues.Plus", "Plus (+)")
  @runtimeProperty("ModSettings.displayValues.Minus", "Minus (-)")
  @runtimeProperty("ModSettings.displayValues.DialRight", "Red dial right")
  @runtimeProperty("ModSettings.displayValues.DialLeft", "Red dial left")
  @runtimeProperty("ModSettings.displayValues.DialPress", "Red dial press")
  @runtimeProperty("ModSettings.displayValues.PS", "PS button")
  @runtimeProperty("ModSettings.displayValues.DpadUp", "D-pad up")
  @runtimeProperty("ModSettings.displayValues.DpadRight", "D-pad right")
  @runtimeProperty("ModSettings.displayValues.DpadDown", "D-pad down")
  @runtimeProperty("ModSettings.displayValues.DpadLeft", "D-pad left")
  let btnCallVehicle: GWheelButton = GWheelButton.None;

  // ---- Camera -------------------------------------------------------------

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Buttons: camera")
  @runtimeProperty("ModSettings.category.order", "410")
  @runtimeProperty("ModSettings.displayName", "Cycle camera")
  @runtimeProperty("ModSettings.description", "Next camera view (Q).")
  @runtimeProperty("ModSettings.displayValues.None", "Not bound")
  @runtimeProperty("ModSettings.displayValues.Cross", "Cross (X)")
  @runtimeProperty("ModSettings.displayValues.Square", "Square")
  @runtimeProperty("ModSettings.displayValues.Circle", "Circle")
  @runtimeProperty("ModSettings.displayValues.Triangle", "Triangle")
  @runtimeProperty("ModSettings.displayValues.PaddleRight", "Right paddle")
  @runtimeProperty("ModSettings.displayValues.PaddleLeft", "Left paddle")
  @runtimeProperty("ModSettings.displayValues.R2", "R2")
  @runtimeProperty("ModSettings.displayValues.L2", "L2")
  @runtimeProperty("ModSettings.displayValues.Share", "Share")
  @runtimeProperty("ModSettings.displayValues.Options", "Options")
  @runtimeProperty("ModSettings.displayValues.R3", "R3")
  @runtimeProperty("ModSettings.displayValues.L3", "L3")
  @runtimeProperty("ModSettings.displayValues.Plus", "Plus (+)")
  @runtimeProperty("ModSettings.displayValues.Minus", "Minus (-)")
  @runtimeProperty("ModSettings.displayValues.DialRight", "Red dial right")
  @runtimeProperty("ModSettings.displayValues.DialLeft", "Red dial left")
  @runtimeProperty("ModSettings.displayValues.DialPress", "Red dial press")
  @runtimeProperty("ModSettings.displayValues.PS", "PS button")
  @runtimeProperty("ModSettings.displayValues.DpadUp", "D-pad up")
  @runtimeProperty("ModSettings.displayValues.DpadRight", "D-pad right")
  @runtimeProperty("ModSettings.displayValues.DpadDown", "D-pad down")
  @runtimeProperty("ModSettings.displayValues.DpadLeft", "D-pad left")
  let btnCameraCycleForward: GWheelButton = GWheelButton.None;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Buttons: camera")
  @runtimeProperty("ModSettings.category.order", "410")
  @runtimeProperty("ModSettings.displayName", "Look behind (hold)")
  @runtimeProperty("ModSettings.description", "Rear view while held (middle mouse).")
  @runtimeProperty("ModSettings.displayValues.None", "Not bound")
  @runtimeProperty("ModSettings.displayValues.Cross", "Cross (X)")
  @runtimeProperty("ModSettings.displayValues.Square", "Square")
  @runtimeProperty("ModSettings.displayValues.Circle", "Circle")
  @runtimeProperty("ModSettings.displayValues.Triangle", "Triangle")
  @runtimeProperty("ModSettings.displayValues.PaddleRight", "Right paddle")
  @runtimeProperty("ModSettings.displayValues.PaddleLeft", "Left paddle")
  @runtimeProperty("ModSettings.displayValues.R2", "R2")
  @runtimeProperty("ModSettings.displayValues.L2", "L2")
  @runtimeProperty("ModSettings.displayValues.Share", "Share")
  @runtimeProperty("ModSettings.displayValues.Options", "Options")
  @runtimeProperty("ModSettings.displayValues.R3", "R3")
  @runtimeProperty("ModSettings.displayValues.L3", "L3")
  @runtimeProperty("ModSettings.displayValues.Plus", "Plus (+)")
  @runtimeProperty("ModSettings.displayValues.Minus", "Minus (-)")
  @runtimeProperty("ModSettings.displayValues.DialRight", "Red dial right")
  @runtimeProperty("ModSettings.displayValues.DialLeft", "Red dial left")
  @runtimeProperty("ModSettings.displayValues.DialPress", "Red dial press")
  @runtimeProperty("ModSettings.displayValues.PS", "PS button")
  @runtimeProperty("ModSettings.displayValues.DpadUp", "D-pad up")
  @runtimeProperty("ModSettings.displayValues.DpadRight", "D-pad right")
  @runtimeProperty("ModSettings.displayValues.DpadDown", "D-pad down")
  @runtimeProperty("ModSettings.displayValues.DpadLeft", "D-pad left")
  let btnRearViewCamera: GWheelButton = GWheelButton.None;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Buttons: camera")
  @runtimeProperty("ModSettings.category.order", "410")
  @runtimeProperty("ModSettings.displayName", "Reset camera")
  @runtimeProperty("ModSettings.description", "Recenters the camera (C).")
  @runtimeProperty("ModSettings.displayValues.None", "Not bound")
  @runtimeProperty("ModSettings.displayValues.Cross", "Cross (X)")
  @runtimeProperty("ModSettings.displayValues.Square", "Square")
  @runtimeProperty("ModSettings.displayValues.Circle", "Circle")
  @runtimeProperty("ModSettings.displayValues.Triangle", "Triangle")
  @runtimeProperty("ModSettings.displayValues.PaddleRight", "Right paddle")
  @runtimeProperty("ModSettings.displayValues.PaddleLeft", "Left paddle")
  @runtimeProperty("ModSettings.displayValues.R2", "R2")
  @runtimeProperty("ModSettings.displayValues.L2", "L2")
  @runtimeProperty("ModSettings.displayValues.Share", "Share")
  @runtimeProperty("ModSettings.displayValues.Options", "Options")
  @runtimeProperty("ModSettings.displayValues.R3", "R3")
  @runtimeProperty("ModSettings.displayValues.L3", "L3")
  @runtimeProperty("ModSettings.displayValues.Plus", "Plus (+)")
  @runtimeProperty("ModSettings.displayValues.Minus", "Minus (-)")
  @runtimeProperty("ModSettings.displayValues.DialRight", "Red dial right")
  @runtimeProperty("ModSettings.displayValues.DialLeft", "Red dial left")
  @runtimeProperty("ModSettings.displayValues.DialPress", "Red dial press")
  @runtimeProperty("ModSettings.displayValues.PS", "PS button")
  @runtimeProperty("ModSettings.displayValues.DpadUp", "D-pad up")
  @runtimeProperty("ModSettings.displayValues.DpadRight", "D-pad right")
  @runtimeProperty("ModSettings.displayValues.DpadDown", "D-pad down")
  @runtimeProperty("ModSettings.displayValues.DpadLeft", "D-pad left")
  let btnCameraReset: GWheelButton = GWheelButton.None;

  // ---- Combat -------------------------------------------------------------

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Buttons: combat")
  @runtimeProperty("ModSettings.category.order", "420")
  @runtimeProperty("ModSettings.displayName", "Shoot")
  @runtimeProperty("ModSettings.description", "Primary fire, held (left mouse). Aiming still follows the mouse / camera.")
  @runtimeProperty("ModSettings.displayValues.None", "Not bound")
  @runtimeProperty("ModSettings.displayValues.Cross", "Cross (X)")
  @runtimeProperty("ModSettings.displayValues.Square", "Square")
  @runtimeProperty("ModSettings.displayValues.Circle", "Circle")
  @runtimeProperty("ModSettings.displayValues.Triangle", "Triangle")
  @runtimeProperty("ModSettings.displayValues.PaddleRight", "Right paddle")
  @runtimeProperty("ModSettings.displayValues.PaddleLeft", "Left paddle")
  @runtimeProperty("ModSettings.displayValues.R2", "R2")
  @runtimeProperty("ModSettings.displayValues.L2", "L2")
  @runtimeProperty("ModSettings.displayValues.Share", "Share")
  @runtimeProperty("ModSettings.displayValues.Options", "Options")
  @runtimeProperty("ModSettings.displayValues.R3", "R3")
  @runtimeProperty("ModSettings.displayValues.L3", "L3")
  @runtimeProperty("ModSettings.displayValues.Plus", "Plus (+)")
  @runtimeProperty("ModSettings.displayValues.Minus", "Minus (-)")
  @runtimeProperty("ModSettings.displayValues.DialRight", "Red dial right")
  @runtimeProperty("ModSettings.displayValues.DialLeft", "Red dial left")
  @runtimeProperty("ModSettings.displayValues.DialPress", "Red dial press")
  @runtimeProperty("ModSettings.displayValues.PS", "PS button")
  @runtimeProperty("ModSettings.displayValues.DpadUp", "D-pad up")
  @runtimeProperty("ModSettings.displayValues.DpadRight", "D-pad right")
  @runtimeProperty("ModSettings.displayValues.DpadDown", "D-pad down")
  @runtimeProperty("ModSettings.displayValues.DpadLeft", "D-pad left")
  let btnShootPrimary: GWheelButton = GWheelButton.None;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Buttons: combat")
  @runtimeProperty("ModSettings.category.order", "420")
  @runtimeProperty("ModSettings.displayName", "Aim / secondary fire")
  @runtimeProperty("ModSettings.description", "Held (right mouse).")
  @runtimeProperty("ModSettings.displayValues.None", "Not bound")
  @runtimeProperty("ModSettings.displayValues.Cross", "Cross (X)")
  @runtimeProperty("ModSettings.displayValues.Square", "Square")
  @runtimeProperty("ModSettings.displayValues.Circle", "Circle")
  @runtimeProperty("ModSettings.displayValues.Triangle", "Triangle")
  @runtimeProperty("ModSettings.displayValues.PaddleRight", "Right paddle")
  @runtimeProperty("ModSettings.displayValues.PaddleLeft", "Left paddle")
  @runtimeProperty("ModSettings.displayValues.R2", "R2")
  @runtimeProperty("ModSettings.displayValues.L2", "L2")
  @runtimeProperty("ModSettings.displayValues.Share", "Share")
  @runtimeProperty("ModSettings.displayValues.Options", "Options")
  @runtimeProperty("ModSettings.displayValues.R3", "R3")
  @runtimeProperty("ModSettings.displayValues.L3", "L3")
  @runtimeProperty("ModSettings.displayValues.Plus", "Plus (+)")
  @runtimeProperty("ModSettings.displayValues.Minus", "Minus (-)")
  @runtimeProperty("ModSettings.displayValues.DialRight", "Red dial right")
  @runtimeProperty("ModSettings.displayValues.DialLeft", "Red dial left")
  @runtimeProperty("ModSettings.displayValues.DialPress", "Red dial press")
  @runtimeProperty("ModSettings.displayValues.PS", "PS button")
  @runtimeProperty("ModSettings.displayValues.DpadUp", "D-pad up")
  @runtimeProperty("ModSettings.displayValues.DpadRight", "D-pad right")
  @runtimeProperty("ModSettings.displayValues.DpadDown", "D-pad down")
  @runtimeProperty("ModSettings.displayValues.DpadLeft", "D-pad left")
  let btnShootSecondary: GWheelButton = GWheelButton.None;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Buttons: combat")
  @runtimeProperty("ModSettings.category.order", "420")
  @runtimeProperty("ModSettings.displayName", "Tertiary fire")
  @runtimeProperty("ModSettings.description", "Held (Left Ctrl).")
  @runtimeProperty("ModSettings.displayValues.None", "Not bound")
  @runtimeProperty("ModSettings.displayValues.Cross", "Cross (X)")
  @runtimeProperty("ModSettings.displayValues.Square", "Square")
  @runtimeProperty("ModSettings.displayValues.Circle", "Circle")
  @runtimeProperty("ModSettings.displayValues.Triangle", "Triangle")
  @runtimeProperty("ModSettings.displayValues.PaddleRight", "Right paddle")
  @runtimeProperty("ModSettings.displayValues.PaddleLeft", "Left paddle")
  @runtimeProperty("ModSettings.displayValues.R2", "R2")
  @runtimeProperty("ModSettings.displayValues.L2", "L2")
  @runtimeProperty("ModSettings.displayValues.Share", "Share")
  @runtimeProperty("ModSettings.displayValues.Options", "Options")
  @runtimeProperty("ModSettings.displayValues.R3", "R3")
  @runtimeProperty("ModSettings.displayValues.L3", "L3")
  @runtimeProperty("ModSettings.displayValues.Plus", "Plus (+)")
  @runtimeProperty("ModSettings.displayValues.Minus", "Minus (-)")
  @runtimeProperty("ModSettings.displayValues.DialRight", "Red dial right")
  @runtimeProperty("ModSettings.displayValues.DialLeft", "Red dial left")
  @runtimeProperty("ModSettings.displayValues.DialPress", "Red dial press")
  @runtimeProperty("ModSettings.displayValues.PS", "PS button")
  @runtimeProperty("ModSettings.displayValues.DpadUp", "D-pad up")
  @runtimeProperty("ModSettings.displayValues.DpadRight", "D-pad right")
  @runtimeProperty("ModSettings.displayValues.DpadDown", "D-pad down")
  @runtimeProperty("ModSettings.displayValues.DpadLeft", "D-pad left")
  let btnShootTertiary: GWheelButton = GWheelButton.None;

  // ---- Weapons ------------------------------------------------------------

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Buttons: combat")
  @runtimeProperty("ModSettings.category.order", "420")
  @runtimeProperty("ModSettings.displayName", "Next weapon")
  @runtimeProperty("ModSettings.description", "Mouse wheel up.")
  @runtimeProperty("ModSettings.displayValues.None", "Not bound")
  @runtimeProperty("ModSettings.displayValues.Cross", "Cross (X)")
  @runtimeProperty("ModSettings.displayValues.Square", "Square")
  @runtimeProperty("ModSettings.displayValues.Circle", "Circle")
  @runtimeProperty("ModSettings.displayValues.Triangle", "Triangle")
  @runtimeProperty("ModSettings.displayValues.PaddleRight", "Right paddle")
  @runtimeProperty("ModSettings.displayValues.PaddleLeft", "Left paddle")
  @runtimeProperty("ModSettings.displayValues.R2", "R2")
  @runtimeProperty("ModSettings.displayValues.L2", "L2")
  @runtimeProperty("ModSettings.displayValues.Share", "Share")
  @runtimeProperty("ModSettings.displayValues.Options", "Options")
  @runtimeProperty("ModSettings.displayValues.R3", "R3")
  @runtimeProperty("ModSettings.displayValues.L3", "L3")
  @runtimeProperty("ModSettings.displayValues.Plus", "Plus (+)")
  @runtimeProperty("ModSettings.displayValues.Minus", "Minus (-)")
  @runtimeProperty("ModSettings.displayValues.DialRight", "Red dial right")
  @runtimeProperty("ModSettings.displayValues.DialLeft", "Red dial left")
  @runtimeProperty("ModSettings.displayValues.DialPress", "Red dial press")
  @runtimeProperty("ModSettings.displayValues.PS", "PS button")
  @runtimeProperty("ModSettings.displayValues.DpadUp", "D-pad up")
  @runtimeProperty("ModSettings.displayValues.DpadRight", "D-pad right")
  @runtimeProperty("ModSettings.displayValues.DpadDown", "D-pad down")
  @runtimeProperty("ModSettings.displayValues.DpadLeft", "D-pad left")
  let btnNextWeapon: GWheelButton = GWheelButton.None;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Buttons: combat")
  @runtimeProperty("ModSettings.category.order", "420")
  @runtimeProperty("ModSettings.displayName", "Previous weapon")
  @runtimeProperty("ModSettings.description", "Mouse wheel down.")
  @runtimeProperty("ModSettings.displayValues.None", "Not bound")
  @runtimeProperty("ModSettings.displayValues.Cross", "Cross (X)")
  @runtimeProperty("ModSettings.displayValues.Square", "Square")
  @runtimeProperty("ModSettings.displayValues.Circle", "Circle")
  @runtimeProperty("ModSettings.displayValues.Triangle", "Triangle")
  @runtimeProperty("ModSettings.displayValues.PaddleRight", "Right paddle")
  @runtimeProperty("ModSettings.displayValues.PaddleLeft", "Left paddle")
  @runtimeProperty("ModSettings.displayValues.R2", "R2")
  @runtimeProperty("ModSettings.displayValues.L2", "L2")
  @runtimeProperty("ModSettings.displayValues.Share", "Share")
  @runtimeProperty("ModSettings.displayValues.Options", "Options")
  @runtimeProperty("ModSettings.displayValues.R3", "R3")
  @runtimeProperty("ModSettings.displayValues.L3", "L3")
  @runtimeProperty("ModSettings.displayValues.Plus", "Plus (+)")
  @runtimeProperty("ModSettings.displayValues.Minus", "Minus (-)")
  @runtimeProperty("ModSettings.displayValues.DialRight", "Red dial right")
  @runtimeProperty("ModSettings.displayValues.DialLeft", "Red dial left")
  @runtimeProperty("ModSettings.displayValues.DialPress", "Red dial press")
  @runtimeProperty("ModSettings.displayValues.PS", "PS button")
  @runtimeProperty("ModSettings.displayValues.DpadUp", "D-pad up")
  @runtimeProperty("ModSettings.displayValues.DpadRight", "D-pad right")
  @runtimeProperty("ModSettings.displayValues.DpadDown", "D-pad down")
  @runtimeProperty("ModSettings.displayValues.DpadLeft", "D-pad left")
  let btnPrevWeapon: GWheelButton = GWheelButton.None;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Buttons: combat")
  @runtimeProperty("ModSettings.category.order", "420")
  @runtimeProperty("ModSettings.displayName", "Weapon slot 1")
  @runtimeProperty("ModSettings.description", "(1)")
  @runtimeProperty("ModSettings.displayValues.None", "Not bound")
  @runtimeProperty("ModSettings.displayValues.Cross", "Cross (X)")
  @runtimeProperty("ModSettings.displayValues.Square", "Square")
  @runtimeProperty("ModSettings.displayValues.Circle", "Circle")
  @runtimeProperty("ModSettings.displayValues.Triangle", "Triangle")
  @runtimeProperty("ModSettings.displayValues.PaddleRight", "Right paddle")
  @runtimeProperty("ModSettings.displayValues.PaddleLeft", "Left paddle")
  @runtimeProperty("ModSettings.displayValues.R2", "R2")
  @runtimeProperty("ModSettings.displayValues.L2", "L2")
  @runtimeProperty("ModSettings.displayValues.Share", "Share")
  @runtimeProperty("ModSettings.displayValues.Options", "Options")
  @runtimeProperty("ModSettings.displayValues.R3", "R3")
  @runtimeProperty("ModSettings.displayValues.L3", "L3")
  @runtimeProperty("ModSettings.displayValues.Plus", "Plus (+)")
  @runtimeProperty("ModSettings.displayValues.Minus", "Minus (-)")
  @runtimeProperty("ModSettings.displayValues.DialRight", "Red dial right")
  @runtimeProperty("ModSettings.displayValues.DialLeft", "Red dial left")
  @runtimeProperty("ModSettings.displayValues.DialPress", "Red dial press")
  @runtimeProperty("ModSettings.displayValues.PS", "PS button")
  @runtimeProperty("ModSettings.displayValues.DpadUp", "D-pad up")
  @runtimeProperty("ModSettings.displayValues.DpadRight", "D-pad right")
  @runtimeProperty("ModSettings.displayValues.DpadDown", "D-pad down")
  @runtimeProperty("ModSettings.displayValues.DpadLeft", "D-pad left")
  let btnWeaponSlot1: GWheelButton = GWheelButton.None;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Buttons: combat")
  @runtimeProperty("ModSettings.category.order", "420")
  @runtimeProperty("ModSettings.displayName", "Weapon slot 2")
  @runtimeProperty("ModSettings.description", "(2)")
  @runtimeProperty("ModSettings.displayValues.None", "Not bound")
  @runtimeProperty("ModSettings.displayValues.Cross", "Cross (X)")
  @runtimeProperty("ModSettings.displayValues.Square", "Square")
  @runtimeProperty("ModSettings.displayValues.Circle", "Circle")
  @runtimeProperty("ModSettings.displayValues.Triangle", "Triangle")
  @runtimeProperty("ModSettings.displayValues.PaddleRight", "Right paddle")
  @runtimeProperty("ModSettings.displayValues.PaddleLeft", "Left paddle")
  @runtimeProperty("ModSettings.displayValues.R2", "R2")
  @runtimeProperty("ModSettings.displayValues.L2", "L2")
  @runtimeProperty("ModSettings.displayValues.Share", "Share")
  @runtimeProperty("ModSettings.displayValues.Options", "Options")
  @runtimeProperty("ModSettings.displayValues.R3", "R3")
  @runtimeProperty("ModSettings.displayValues.L3", "L3")
  @runtimeProperty("ModSettings.displayValues.Plus", "Plus (+)")
  @runtimeProperty("ModSettings.displayValues.Minus", "Minus (-)")
  @runtimeProperty("ModSettings.displayValues.DialRight", "Red dial right")
  @runtimeProperty("ModSettings.displayValues.DialLeft", "Red dial left")
  @runtimeProperty("ModSettings.displayValues.DialPress", "Red dial press")
  @runtimeProperty("ModSettings.displayValues.PS", "PS button")
  @runtimeProperty("ModSettings.displayValues.DpadUp", "D-pad up")
  @runtimeProperty("ModSettings.displayValues.DpadRight", "D-pad right")
  @runtimeProperty("ModSettings.displayValues.DpadDown", "D-pad down")
  @runtimeProperty("ModSettings.displayValues.DpadLeft", "D-pad left")
  let btnWeaponSlot2: GWheelButton = GWheelButton.None;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Buttons: combat")
  @runtimeProperty("ModSettings.category.order", "420")
  @runtimeProperty("ModSettings.displayName", "Weapon wheel (hold)")
  @runtimeProperty("ModSettings.description", "Held (Alt). Choosing from the wheel still needs the mouse.")
  @runtimeProperty("ModSettings.displayValues.None", "Not bound")
  @runtimeProperty("ModSettings.displayValues.Cross", "Cross (X)")
  @runtimeProperty("ModSettings.displayValues.Square", "Square")
  @runtimeProperty("ModSettings.displayValues.Circle", "Circle")
  @runtimeProperty("ModSettings.displayValues.Triangle", "Triangle")
  @runtimeProperty("ModSettings.displayValues.PaddleRight", "Right paddle")
  @runtimeProperty("ModSettings.displayValues.PaddleLeft", "Left paddle")
  @runtimeProperty("ModSettings.displayValues.R2", "R2")
  @runtimeProperty("ModSettings.displayValues.L2", "L2")
  @runtimeProperty("ModSettings.displayValues.Share", "Share")
  @runtimeProperty("ModSettings.displayValues.Options", "Options")
  @runtimeProperty("ModSettings.displayValues.R3", "R3")
  @runtimeProperty("ModSettings.displayValues.L3", "L3")
  @runtimeProperty("ModSettings.displayValues.Plus", "Plus (+)")
  @runtimeProperty("ModSettings.displayValues.Minus", "Minus (-)")
  @runtimeProperty("ModSettings.displayValues.DialRight", "Red dial right")
  @runtimeProperty("ModSettings.displayValues.DialLeft", "Red dial left")
  @runtimeProperty("ModSettings.displayValues.DialPress", "Red dial press")
  @runtimeProperty("ModSettings.displayValues.PS", "PS button")
  @runtimeProperty("ModSettings.displayValues.DpadUp", "D-pad up")
  @runtimeProperty("ModSettings.displayValues.DpadRight", "D-pad right")
  @runtimeProperty("ModSettings.displayValues.DpadDown", "D-pad down")
  @runtimeProperty("ModSettings.displayValues.DpadLeft", "D-pad left")
  let btnSwitchWeapons: GWheelButton = GWheelButton.None;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Buttons: combat")
  @runtimeProperty("ModSettings.category.order", "420")
  @runtimeProperty("ModSettings.displayName", "Holster weapon")
  @runtimeProperty("ModSettings.description", "(B)")
  @runtimeProperty("ModSettings.displayValues.None", "Not bound")
  @runtimeProperty("ModSettings.displayValues.Cross", "Cross (X)")
  @runtimeProperty("ModSettings.displayValues.Square", "Square")
  @runtimeProperty("ModSettings.displayValues.Circle", "Circle")
  @runtimeProperty("ModSettings.displayValues.Triangle", "Triangle")
  @runtimeProperty("ModSettings.displayValues.PaddleRight", "Right paddle")
  @runtimeProperty("ModSettings.displayValues.PaddleLeft", "Left paddle")
  @runtimeProperty("ModSettings.displayValues.R2", "R2")
  @runtimeProperty("ModSettings.displayValues.L2", "L2")
  @runtimeProperty("ModSettings.displayValues.Share", "Share")
  @runtimeProperty("ModSettings.displayValues.Options", "Options")
  @runtimeProperty("ModSettings.displayValues.R3", "R3")
  @runtimeProperty("ModSettings.displayValues.L3", "L3")
  @runtimeProperty("ModSettings.displayValues.Plus", "Plus (+)")
  @runtimeProperty("ModSettings.displayValues.Minus", "Minus (-)")
  @runtimeProperty("ModSettings.displayValues.DialRight", "Red dial right")
  @runtimeProperty("ModSettings.displayValues.DialLeft", "Red dial left")
  @runtimeProperty("ModSettings.displayValues.DialPress", "Red dial press")
  @runtimeProperty("ModSettings.displayValues.PS", "PS button")
  @runtimeProperty("ModSettings.displayValues.DpadUp", "D-pad up")
  @runtimeProperty("ModSettings.displayValues.DpadRight", "D-pad right")
  @runtimeProperty("ModSettings.displayValues.DpadDown", "D-pad down")
  @runtimeProperty("ModSettings.displayValues.DpadLeft", "D-pad left")
  let btnHolsterWeapon: GWheelButton = GWheelButton.None;

  // ---- Radio --------------------------------------------------------------

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Buttons: radio and items")
  @runtimeProperty("ModSettings.category.order", "430")
  @runtimeProperty("ModSettings.displayName", "Radio")
  @runtimeProperty("ModSettings.description", "Tap for next station, hold for the radio menu (R).")
  @runtimeProperty("ModSettings.displayValues.None", "Not bound")
  @runtimeProperty("ModSettings.displayValues.Cross", "Cross (X)")
  @runtimeProperty("ModSettings.displayValues.Square", "Square")
  @runtimeProperty("ModSettings.displayValues.Circle", "Circle")
  @runtimeProperty("ModSettings.displayValues.Triangle", "Triangle")
  @runtimeProperty("ModSettings.displayValues.PaddleRight", "Right paddle")
  @runtimeProperty("ModSettings.displayValues.PaddleLeft", "Left paddle")
  @runtimeProperty("ModSettings.displayValues.R2", "R2")
  @runtimeProperty("ModSettings.displayValues.L2", "L2")
  @runtimeProperty("ModSettings.displayValues.Share", "Share")
  @runtimeProperty("ModSettings.displayValues.Options", "Options")
  @runtimeProperty("ModSettings.displayValues.R3", "R3")
  @runtimeProperty("ModSettings.displayValues.L3", "L3")
  @runtimeProperty("ModSettings.displayValues.Plus", "Plus (+)")
  @runtimeProperty("ModSettings.displayValues.Minus", "Minus (-)")
  @runtimeProperty("ModSettings.displayValues.DialRight", "Red dial right")
  @runtimeProperty("ModSettings.displayValues.DialLeft", "Red dial left")
  @runtimeProperty("ModSettings.displayValues.DialPress", "Red dial press")
  @runtimeProperty("ModSettings.displayValues.PS", "PS button")
  @runtimeProperty("ModSettings.displayValues.DpadUp", "D-pad up")
  @runtimeProperty("ModSettings.displayValues.DpadRight", "D-pad right")
  @runtimeProperty("ModSettings.displayValues.DpadDown", "D-pad down")
  @runtimeProperty("ModSettings.displayValues.DpadLeft", "D-pad left")
  let btnRadioMenu: GWheelButton = GWheelButton.None;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Buttons: radio and items")
  @runtimeProperty("ModSettings.category.order", "430")
  @runtimeProperty("ModSettings.displayName", "Next radio station")
  @runtimeProperty("ModSettings.description", "Switches station directly.")
  @runtimeProperty("ModSettings.displayValues.None", "Not bound")
  @runtimeProperty("ModSettings.displayValues.Cross", "Cross (X)")
  @runtimeProperty("ModSettings.displayValues.Square", "Square")
  @runtimeProperty("ModSettings.displayValues.Circle", "Circle")
  @runtimeProperty("ModSettings.displayValues.Triangle", "Triangle")
  @runtimeProperty("ModSettings.displayValues.PaddleRight", "Right paddle")
  @runtimeProperty("ModSettings.displayValues.PaddleLeft", "Left paddle")
  @runtimeProperty("ModSettings.displayValues.R2", "R2")
  @runtimeProperty("ModSettings.displayValues.L2", "L2")
  @runtimeProperty("ModSettings.displayValues.Share", "Share")
  @runtimeProperty("ModSettings.displayValues.Options", "Options")
  @runtimeProperty("ModSettings.displayValues.R3", "R3")
  @runtimeProperty("ModSettings.displayValues.L3", "L3")
  @runtimeProperty("ModSettings.displayValues.Plus", "Plus (+)")
  @runtimeProperty("ModSettings.displayValues.Minus", "Minus (-)")
  @runtimeProperty("ModSettings.displayValues.DialRight", "Red dial right")
  @runtimeProperty("ModSettings.displayValues.DialLeft", "Red dial left")
  @runtimeProperty("ModSettings.displayValues.DialPress", "Red dial press")
  @runtimeProperty("ModSettings.displayValues.PS", "PS button")
  @runtimeProperty("ModSettings.displayValues.DpadUp", "D-pad up")
  @runtimeProperty("ModSettings.displayValues.DpadRight", "D-pad right")
  @runtimeProperty("ModSettings.displayValues.DpadDown", "D-pad down")
  @runtimeProperty("ModSettings.displayValues.DpadLeft", "D-pad left")
  let btnRadioNext: GWheelButton = GWheelButton.None;

  // ---- Gameplay -----------------------------------------------------------

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Buttons: radio and items")
  @runtimeProperty("ModSettings.category.order", "430")
  @runtimeProperty("ModSettings.displayName", "Use consumable")
  @runtimeProperty("ModSettings.description", "(X)")
  @runtimeProperty("ModSettings.displayValues.None", "Not bound")
  @runtimeProperty("ModSettings.displayValues.Cross", "Cross (X)")
  @runtimeProperty("ModSettings.displayValues.Square", "Square")
  @runtimeProperty("ModSettings.displayValues.Circle", "Circle")
  @runtimeProperty("ModSettings.displayValues.Triangle", "Triangle")
  @runtimeProperty("ModSettings.displayValues.PaddleRight", "Right paddle")
  @runtimeProperty("ModSettings.displayValues.PaddleLeft", "Left paddle")
  @runtimeProperty("ModSettings.displayValues.R2", "R2")
  @runtimeProperty("ModSettings.displayValues.L2", "L2")
  @runtimeProperty("ModSettings.displayValues.Share", "Share")
  @runtimeProperty("ModSettings.displayValues.Options", "Options")
  @runtimeProperty("ModSettings.displayValues.R3", "R3")
  @runtimeProperty("ModSettings.displayValues.L3", "L3")
  @runtimeProperty("ModSettings.displayValues.Plus", "Plus (+)")
  @runtimeProperty("ModSettings.displayValues.Minus", "Minus (-)")
  @runtimeProperty("ModSettings.displayValues.DialRight", "Red dial right")
  @runtimeProperty("ModSettings.displayValues.DialLeft", "Red dial left")
  @runtimeProperty("ModSettings.displayValues.DialPress", "Red dial press")
  @runtimeProperty("ModSettings.displayValues.PS", "PS button")
  @runtimeProperty("ModSettings.displayValues.DpadUp", "D-pad up")
  @runtimeProperty("ModSettings.displayValues.DpadRight", "D-pad right")
  @runtimeProperty("ModSettings.displayValues.DpadDown", "D-pad down")
  @runtimeProperty("ModSettings.displayValues.DpadLeft", "D-pad left")
  let btnUseConsumable: GWheelButton = GWheelButton.None;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Buttons: radio and items")
  @runtimeProperty("ModSettings.category.order", "430")
  @runtimeProperty("ModSettings.displayName", "Iconic cyberware")
  @runtimeProperty("ModSettings.description", "(E)")
  @runtimeProperty("ModSettings.displayValues.None", "Not bound")
  @runtimeProperty("ModSettings.displayValues.Cross", "Cross (X)")
  @runtimeProperty("ModSettings.displayValues.Square", "Square")
  @runtimeProperty("ModSettings.displayValues.Circle", "Circle")
  @runtimeProperty("ModSettings.displayValues.Triangle", "Triangle")
  @runtimeProperty("ModSettings.displayValues.PaddleRight", "Right paddle")
  @runtimeProperty("ModSettings.displayValues.PaddleLeft", "Left paddle")
  @runtimeProperty("ModSettings.displayValues.R2", "R2")
  @runtimeProperty("ModSettings.displayValues.L2", "L2")
  @runtimeProperty("ModSettings.displayValues.Share", "Share")
  @runtimeProperty("ModSettings.displayValues.Options", "Options")
  @runtimeProperty("ModSettings.displayValues.R3", "R3")
  @runtimeProperty("ModSettings.displayValues.L3", "L3")
  @runtimeProperty("ModSettings.displayValues.Plus", "Plus (+)")
  @runtimeProperty("ModSettings.displayValues.Minus", "Minus (-)")
  @runtimeProperty("ModSettings.displayValues.DialRight", "Red dial right")
  @runtimeProperty("ModSettings.displayValues.DialLeft", "Red dial left")
  @runtimeProperty("ModSettings.displayValues.DialPress", "Red dial press")
  @runtimeProperty("ModSettings.displayValues.PS", "PS button")
  @runtimeProperty("ModSettings.displayValues.DpadUp", "D-pad up")
  @runtimeProperty("ModSettings.displayValues.DpadRight", "D-pad right")
  @runtimeProperty("ModSettings.displayValues.DpadDown", "D-pad down")
  @runtimeProperty("ModSettings.displayValues.DpadLeft", "D-pad left")
  let btnIconicCyberware: GWheelButton = GWheelButton.None;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Buttons: radio and items")
  @runtimeProperty("ModSettings.category.order", "430")
  @runtimeProperty("ModSettings.displayName", "Quick save")
  @runtimeProperty("ModSettings.description", "(F5)")
  @runtimeProperty("ModSettings.displayValues.None", "Not bound")
  @runtimeProperty("ModSettings.displayValues.Cross", "Cross (X)")
  @runtimeProperty("ModSettings.displayValues.Square", "Square")
  @runtimeProperty("ModSettings.displayValues.Circle", "Circle")
  @runtimeProperty("ModSettings.displayValues.Triangle", "Triangle")
  @runtimeProperty("ModSettings.displayValues.PaddleRight", "Right paddle")
  @runtimeProperty("ModSettings.displayValues.PaddleLeft", "Left paddle")
  @runtimeProperty("ModSettings.displayValues.R2", "R2")
  @runtimeProperty("ModSettings.displayValues.L2", "L2")
  @runtimeProperty("ModSettings.displayValues.Share", "Share")
  @runtimeProperty("ModSettings.displayValues.Options", "Options")
  @runtimeProperty("ModSettings.displayValues.R3", "R3")
  @runtimeProperty("ModSettings.displayValues.L3", "L3")
  @runtimeProperty("ModSettings.displayValues.Plus", "Plus (+)")
  @runtimeProperty("ModSettings.displayValues.Minus", "Minus (-)")
  @runtimeProperty("ModSettings.displayValues.DialRight", "Red dial right")
  @runtimeProperty("ModSettings.displayValues.DialLeft", "Red dial left")
  @runtimeProperty("ModSettings.displayValues.DialPress", "Red dial press")
  @runtimeProperty("ModSettings.displayValues.PS", "PS button")
  @runtimeProperty("ModSettings.displayValues.DpadUp", "D-pad up")
  @runtimeProperty("ModSettings.displayValues.DpadRight", "D-pad right")
  @runtimeProperty("ModSettings.displayValues.DpadDown", "D-pad down")
  @runtimeProperty("ModSettings.displayValues.DpadLeft", "D-pad left")
  let btnQuickSave: GWheelButton = GWheelButton.None;

  // ---- Menus --------------------------------------------------------------

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Buttons: menus")
  @runtimeProperty("ModSettings.category.order", "440")
  @runtimeProperty("ModSettings.displayName", "Map")
  @runtimeProperty("ModSettings.description", "(M)")
  @runtimeProperty("ModSettings.displayValues.None", "Not bound")
  @runtimeProperty("ModSettings.displayValues.Cross", "Cross (X)")
  @runtimeProperty("ModSettings.displayValues.Square", "Square")
  @runtimeProperty("ModSettings.displayValues.Circle", "Circle")
  @runtimeProperty("ModSettings.displayValues.Triangle", "Triangle")
  @runtimeProperty("ModSettings.displayValues.PaddleRight", "Right paddle")
  @runtimeProperty("ModSettings.displayValues.PaddleLeft", "Left paddle")
  @runtimeProperty("ModSettings.displayValues.R2", "R2")
  @runtimeProperty("ModSettings.displayValues.L2", "L2")
  @runtimeProperty("ModSettings.displayValues.Share", "Share")
  @runtimeProperty("ModSettings.displayValues.Options", "Options")
  @runtimeProperty("ModSettings.displayValues.R3", "R3")
  @runtimeProperty("ModSettings.displayValues.L3", "L3")
  @runtimeProperty("ModSettings.displayValues.Plus", "Plus (+)")
  @runtimeProperty("ModSettings.displayValues.Minus", "Minus (-)")
  @runtimeProperty("ModSettings.displayValues.DialRight", "Red dial right")
  @runtimeProperty("ModSettings.displayValues.DialLeft", "Red dial left")
  @runtimeProperty("ModSettings.displayValues.DialPress", "Red dial press")
  @runtimeProperty("ModSettings.displayValues.PS", "PS button")
  @runtimeProperty("ModSettings.displayValues.DpadUp", "D-pad up")
  @runtimeProperty("ModSettings.displayValues.DpadRight", "D-pad right")
  @runtimeProperty("ModSettings.displayValues.DpadDown", "D-pad down")
  @runtimeProperty("ModSettings.displayValues.DpadLeft", "D-pad left")
  let btnOpenMap: GWheelButton = GWheelButton.None;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Buttons: menus")
  @runtimeProperty("ModSettings.category.order", "440")
  @runtimeProperty("ModSettings.displayName", "Journal")
  @runtimeProperty("ModSettings.description", "(J)")
  @runtimeProperty("ModSettings.displayValues.None", "Not bound")
  @runtimeProperty("ModSettings.displayValues.Cross", "Cross (X)")
  @runtimeProperty("ModSettings.displayValues.Square", "Square")
  @runtimeProperty("ModSettings.displayValues.Circle", "Circle")
  @runtimeProperty("ModSettings.displayValues.Triangle", "Triangle")
  @runtimeProperty("ModSettings.displayValues.PaddleRight", "Right paddle")
  @runtimeProperty("ModSettings.displayValues.PaddleLeft", "Left paddle")
  @runtimeProperty("ModSettings.displayValues.R2", "R2")
  @runtimeProperty("ModSettings.displayValues.L2", "L2")
  @runtimeProperty("ModSettings.displayValues.Share", "Share")
  @runtimeProperty("ModSettings.displayValues.Options", "Options")
  @runtimeProperty("ModSettings.displayValues.R3", "R3")
  @runtimeProperty("ModSettings.displayValues.L3", "L3")
  @runtimeProperty("ModSettings.displayValues.Plus", "Plus (+)")
  @runtimeProperty("ModSettings.displayValues.Minus", "Minus (-)")
  @runtimeProperty("ModSettings.displayValues.DialRight", "Red dial right")
  @runtimeProperty("ModSettings.displayValues.DialLeft", "Red dial left")
  @runtimeProperty("ModSettings.displayValues.DialPress", "Red dial press")
  @runtimeProperty("ModSettings.displayValues.PS", "PS button")
  @runtimeProperty("ModSettings.displayValues.DpadUp", "D-pad up")
  @runtimeProperty("ModSettings.displayValues.DpadRight", "D-pad right")
  @runtimeProperty("ModSettings.displayValues.DpadDown", "D-pad down")
  @runtimeProperty("ModSettings.displayValues.DpadLeft", "D-pad left")
  let btnOpenJournal: GWheelButton = GWheelButton.None;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Buttons: menus")
  @runtimeProperty("ModSettings.category.order", "440")
  @runtimeProperty("ModSettings.displayName", "Inventory")
  @runtimeProperty("ModSettings.description", "(I)")
  @runtimeProperty("ModSettings.displayValues.None", "Not bound")
  @runtimeProperty("ModSettings.displayValues.Cross", "Cross (X)")
  @runtimeProperty("ModSettings.displayValues.Square", "Square")
  @runtimeProperty("ModSettings.displayValues.Circle", "Circle")
  @runtimeProperty("ModSettings.displayValues.Triangle", "Triangle")
  @runtimeProperty("ModSettings.displayValues.PaddleRight", "Right paddle")
  @runtimeProperty("ModSettings.displayValues.PaddleLeft", "Left paddle")
  @runtimeProperty("ModSettings.displayValues.R2", "R2")
  @runtimeProperty("ModSettings.displayValues.L2", "L2")
  @runtimeProperty("ModSettings.displayValues.Share", "Share")
  @runtimeProperty("ModSettings.displayValues.Options", "Options")
  @runtimeProperty("ModSettings.displayValues.R3", "R3")
  @runtimeProperty("ModSettings.displayValues.L3", "L3")
  @runtimeProperty("ModSettings.displayValues.Plus", "Plus (+)")
  @runtimeProperty("ModSettings.displayValues.Minus", "Minus (-)")
  @runtimeProperty("ModSettings.displayValues.DialRight", "Red dial right")
  @runtimeProperty("ModSettings.displayValues.DialLeft", "Red dial left")
  @runtimeProperty("ModSettings.displayValues.DialPress", "Red dial press")
  @runtimeProperty("ModSettings.displayValues.PS", "PS button")
  @runtimeProperty("ModSettings.displayValues.DpadUp", "D-pad up")
  @runtimeProperty("ModSettings.displayValues.DpadRight", "D-pad right")
  @runtimeProperty("ModSettings.displayValues.DpadDown", "D-pad down")
  @runtimeProperty("ModSettings.displayValues.DpadLeft", "D-pad left")
  let btnOpenInventory: GWheelButton = GWheelButton.None;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Buttons: menus")
  @runtimeProperty("ModSettings.category.order", "440")
  @runtimeProperty("ModSettings.displayName", "Phone")
  @runtimeProperty("ModSettings.description", "Tap answers a call, hold opens the phone (T).")
  @runtimeProperty("ModSettings.displayValues.None", "Not bound")
  @runtimeProperty("ModSettings.displayValues.Cross", "Cross (X)")
  @runtimeProperty("ModSettings.displayValues.Square", "Square")
  @runtimeProperty("ModSettings.displayValues.Circle", "Circle")
  @runtimeProperty("ModSettings.displayValues.Triangle", "Triangle")
  @runtimeProperty("ModSettings.displayValues.PaddleRight", "Right paddle")
  @runtimeProperty("ModSettings.displayValues.PaddleLeft", "Left paddle")
  @runtimeProperty("ModSettings.displayValues.R2", "R2")
  @runtimeProperty("ModSettings.displayValues.L2", "L2")
  @runtimeProperty("ModSettings.displayValues.Share", "Share")
  @runtimeProperty("ModSettings.displayValues.Options", "Options")
  @runtimeProperty("ModSettings.displayValues.R3", "R3")
  @runtimeProperty("ModSettings.displayValues.L3", "L3")
  @runtimeProperty("ModSettings.displayValues.Plus", "Plus (+)")
  @runtimeProperty("ModSettings.displayValues.Minus", "Minus (-)")
  @runtimeProperty("ModSettings.displayValues.DialRight", "Red dial right")
  @runtimeProperty("ModSettings.displayValues.DialLeft", "Red dial left")
  @runtimeProperty("ModSettings.displayValues.DialPress", "Red dial press")
  @runtimeProperty("ModSettings.displayValues.PS", "PS button")
  @runtimeProperty("ModSettings.displayValues.DpadUp", "D-pad up")
  @runtimeProperty("ModSettings.displayValues.DpadRight", "D-pad right")
  @runtimeProperty("ModSettings.displayValues.DpadDown", "D-pad down")
  @runtimeProperty("ModSettings.displayValues.DpadLeft", "D-pad left")
  let btnOpenPhone: GWheelButton = GWheelButton.None;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Buttons: menus")
  @runtimeProperty("ModSettings.category.order", "440")
  @runtimeProperty("ModSettings.displayName", "Perks")
  @runtimeProperty("ModSettings.description", "(P)")
  @runtimeProperty("ModSettings.displayValues.None", "Not bound")
  @runtimeProperty("ModSettings.displayValues.Cross", "Cross (X)")
  @runtimeProperty("ModSettings.displayValues.Square", "Square")
  @runtimeProperty("ModSettings.displayValues.Circle", "Circle")
  @runtimeProperty("ModSettings.displayValues.Triangle", "Triangle")
  @runtimeProperty("ModSettings.displayValues.PaddleRight", "Right paddle")
  @runtimeProperty("ModSettings.displayValues.PaddleLeft", "Left paddle")
  @runtimeProperty("ModSettings.displayValues.R2", "R2")
  @runtimeProperty("ModSettings.displayValues.L2", "L2")
  @runtimeProperty("ModSettings.displayValues.Share", "Share")
  @runtimeProperty("ModSettings.displayValues.Options", "Options")
  @runtimeProperty("ModSettings.displayValues.R3", "R3")
  @runtimeProperty("ModSettings.displayValues.L3", "L3")
  @runtimeProperty("ModSettings.displayValues.Plus", "Plus (+)")
  @runtimeProperty("ModSettings.displayValues.Minus", "Minus (-)")
  @runtimeProperty("ModSettings.displayValues.DialRight", "Red dial right")
  @runtimeProperty("ModSettings.displayValues.DialLeft", "Red dial left")
  @runtimeProperty("ModSettings.displayValues.DialPress", "Red dial press")
  @runtimeProperty("ModSettings.displayValues.PS", "PS button")
  @runtimeProperty("ModSettings.displayValues.DpadUp", "D-pad up")
  @runtimeProperty("ModSettings.displayValues.DpadRight", "D-pad right")
  @runtimeProperty("ModSettings.displayValues.DpadDown", "D-pad down")
  @runtimeProperty("ModSettings.displayValues.DpadLeft", "D-pad left")
  let btnOpenPerks: GWheelButton = GWheelButton.None;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Buttons: menus")
  @runtimeProperty("ModSettings.category.order", "440")
  @runtimeProperty("ModSettings.displayName", "Crafting")
  @runtimeProperty("ModSettings.displayValues.None", "Not bound")
  @runtimeProperty("ModSettings.displayValues.Cross", "Cross (X)")
  @runtimeProperty("ModSettings.displayValues.Square", "Square")
  @runtimeProperty("ModSettings.displayValues.Circle", "Circle")
  @runtimeProperty("ModSettings.displayValues.Triangle", "Triangle")
  @runtimeProperty("ModSettings.displayValues.PaddleRight", "Right paddle")
  @runtimeProperty("ModSettings.displayValues.PaddleLeft", "Left paddle")
  @runtimeProperty("ModSettings.displayValues.R2", "R2")
  @runtimeProperty("ModSettings.displayValues.L2", "L2")
  @runtimeProperty("ModSettings.displayValues.Share", "Share")
  @runtimeProperty("ModSettings.displayValues.Options", "Options")
  @runtimeProperty("ModSettings.displayValues.R3", "R3")
  @runtimeProperty("ModSettings.displayValues.L3", "L3")
  @runtimeProperty("ModSettings.displayValues.Plus", "Plus (+)")
  @runtimeProperty("ModSettings.displayValues.Minus", "Minus (-)")
  @runtimeProperty("ModSettings.displayValues.DialRight", "Red dial right")
  @runtimeProperty("ModSettings.displayValues.DialLeft", "Red dial left")
  @runtimeProperty("ModSettings.displayValues.DialPress", "Red dial press")
  @runtimeProperty("ModSettings.displayValues.PS", "PS button")
  @runtimeProperty("ModSettings.displayValues.DpadUp", "D-pad up")
  @runtimeProperty("ModSettings.displayValues.DpadRight", "D-pad right")
  @runtimeProperty("ModSettings.displayValues.DpadDown", "D-pad down")
  @runtimeProperty("ModSettings.displayValues.DpadLeft", "D-pad left")
  let btnOpenCrafting: GWheelButton = GWheelButton.None;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Buttons: menus")
  @runtimeProperty("ModSettings.category.order", "440")
  @runtimeProperty("ModSettings.displayName", "Pause")
  @runtimeProperty("ModSettings.description", "(Esc)")
  @runtimeProperty("ModSettings.displayValues.None", "Not bound")
  @runtimeProperty("ModSettings.displayValues.Cross", "Cross (X)")
  @runtimeProperty("ModSettings.displayValues.Square", "Square")
  @runtimeProperty("ModSettings.displayValues.Circle", "Circle")
  @runtimeProperty("ModSettings.displayValues.Triangle", "Triangle")
  @runtimeProperty("ModSettings.displayValues.PaddleRight", "Right paddle")
  @runtimeProperty("ModSettings.displayValues.PaddleLeft", "Left paddle")
  @runtimeProperty("ModSettings.displayValues.R2", "R2")
  @runtimeProperty("ModSettings.displayValues.L2", "L2")
  @runtimeProperty("ModSettings.displayValues.Share", "Share")
  @runtimeProperty("ModSettings.displayValues.Options", "Options")
  @runtimeProperty("ModSettings.displayValues.R3", "R3")
  @runtimeProperty("ModSettings.displayValues.L3", "L3")
  @runtimeProperty("ModSettings.displayValues.Plus", "Plus (+)")
  @runtimeProperty("ModSettings.displayValues.Minus", "Minus (-)")
  @runtimeProperty("ModSettings.displayValues.DialRight", "Red dial right")
  @runtimeProperty("ModSettings.displayValues.DialLeft", "Red dial left")
  @runtimeProperty("ModSettings.displayValues.DialPress", "Red dial press")
  @runtimeProperty("ModSettings.displayValues.PS", "PS button")
  @runtimeProperty("ModSettings.displayValues.DpadUp", "D-pad up")
  @runtimeProperty("ModSettings.displayValues.DpadRight", "D-pad right")
  @runtimeProperty("ModSettings.displayValues.DpadDown", "D-pad down")
  @runtimeProperty("ModSettings.displayValues.DpadLeft", "D-pad left")
  let btnPause: GWheelButton = GWheelButton.None;

  // ---- Menu navigation ----------------------------------------------------

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Buttons: menus")
  @runtimeProperty("ModSettings.category.order", "440")
  @runtimeProperty("ModSettings.displayName", "Menu confirm")
  @runtimeProperty("ModSettings.displayValues.None", "Not bound")
  @runtimeProperty("ModSettings.displayValues.Cross", "Cross (X)")
  @runtimeProperty("ModSettings.displayValues.Square", "Square")
  @runtimeProperty("ModSettings.displayValues.Circle", "Circle")
  @runtimeProperty("ModSettings.displayValues.Triangle", "Triangle")
  @runtimeProperty("ModSettings.displayValues.PaddleRight", "Right paddle")
  @runtimeProperty("ModSettings.displayValues.PaddleLeft", "Left paddle")
  @runtimeProperty("ModSettings.displayValues.R2", "R2")
  @runtimeProperty("ModSettings.displayValues.L2", "L2")
  @runtimeProperty("ModSettings.displayValues.Share", "Share")
  @runtimeProperty("ModSettings.displayValues.Options", "Options")
  @runtimeProperty("ModSettings.displayValues.R3", "R3")
  @runtimeProperty("ModSettings.displayValues.L3", "L3")
  @runtimeProperty("ModSettings.displayValues.Plus", "Plus (+)")
  @runtimeProperty("ModSettings.displayValues.Minus", "Minus (-)")
  @runtimeProperty("ModSettings.displayValues.DialRight", "Red dial right")
  @runtimeProperty("ModSettings.displayValues.DialLeft", "Red dial left")
  @runtimeProperty("ModSettings.displayValues.DialPress", "Red dial press")
  @runtimeProperty("ModSettings.displayValues.PS", "PS button")
  @runtimeProperty("ModSettings.displayValues.DpadUp", "D-pad up")
  @runtimeProperty("ModSettings.displayValues.DpadRight", "D-pad right")
  @runtimeProperty("ModSettings.displayValues.DpadDown", "D-pad down")
  @runtimeProperty("ModSettings.displayValues.DpadLeft", "D-pad left")
  let btnMenuConfirm: GWheelButton = GWheelButton.None;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Buttons: menus")
  @runtimeProperty("ModSettings.category.order", "440")
  @runtimeProperty("ModSettings.displayName", "Menu back")
  @runtimeProperty("ModSettings.displayValues.None", "Not bound")
  @runtimeProperty("ModSettings.displayValues.Cross", "Cross (X)")
  @runtimeProperty("ModSettings.displayValues.Square", "Square")
  @runtimeProperty("ModSettings.displayValues.Circle", "Circle")
  @runtimeProperty("ModSettings.displayValues.Triangle", "Triangle")
  @runtimeProperty("ModSettings.displayValues.PaddleRight", "Right paddle")
  @runtimeProperty("ModSettings.displayValues.PaddleLeft", "Left paddle")
  @runtimeProperty("ModSettings.displayValues.R2", "R2")
  @runtimeProperty("ModSettings.displayValues.L2", "L2")
  @runtimeProperty("ModSettings.displayValues.Share", "Share")
  @runtimeProperty("ModSettings.displayValues.Options", "Options")
  @runtimeProperty("ModSettings.displayValues.R3", "R3")
  @runtimeProperty("ModSettings.displayValues.L3", "L3")
  @runtimeProperty("ModSettings.displayValues.Plus", "Plus (+)")
  @runtimeProperty("ModSettings.displayValues.Minus", "Minus (-)")
  @runtimeProperty("ModSettings.displayValues.DialRight", "Red dial right")
  @runtimeProperty("ModSettings.displayValues.DialLeft", "Red dial left")
  @runtimeProperty("ModSettings.displayValues.DialPress", "Red dial press")
  @runtimeProperty("ModSettings.displayValues.PS", "PS button")
  @runtimeProperty("ModSettings.displayValues.DpadUp", "D-pad up")
  @runtimeProperty("ModSettings.displayValues.DpadRight", "D-pad right")
  @runtimeProperty("ModSettings.displayValues.DpadDown", "D-pad down")
  @runtimeProperty("ModSettings.displayValues.DpadLeft", "D-pad left")
  let btnMenuCancel: GWheelButton = GWheelButton.None;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Buttons: menus")
  @runtimeProperty("ModSettings.category.order", "440")
  @runtimeProperty("ModSettings.displayName", "Menu up")
  @runtimeProperty("ModSettings.displayValues.None", "Not bound")
  @runtimeProperty("ModSettings.displayValues.Cross", "Cross (X)")
  @runtimeProperty("ModSettings.displayValues.Square", "Square")
  @runtimeProperty("ModSettings.displayValues.Circle", "Circle")
  @runtimeProperty("ModSettings.displayValues.Triangle", "Triangle")
  @runtimeProperty("ModSettings.displayValues.PaddleRight", "Right paddle")
  @runtimeProperty("ModSettings.displayValues.PaddleLeft", "Left paddle")
  @runtimeProperty("ModSettings.displayValues.R2", "R2")
  @runtimeProperty("ModSettings.displayValues.L2", "L2")
  @runtimeProperty("ModSettings.displayValues.Share", "Share")
  @runtimeProperty("ModSettings.displayValues.Options", "Options")
  @runtimeProperty("ModSettings.displayValues.R3", "R3")
  @runtimeProperty("ModSettings.displayValues.L3", "L3")
  @runtimeProperty("ModSettings.displayValues.Plus", "Plus (+)")
  @runtimeProperty("ModSettings.displayValues.Minus", "Minus (-)")
  @runtimeProperty("ModSettings.displayValues.DialRight", "Red dial right")
  @runtimeProperty("ModSettings.displayValues.DialLeft", "Red dial left")
  @runtimeProperty("ModSettings.displayValues.DialPress", "Red dial press")
  @runtimeProperty("ModSettings.displayValues.PS", "PS button")
  @runtimeProperty("ModSettings.displayValues.DpadUp", "D-pad up")
  @runtimeProperty("ModSettings.displayValues.DpadRight", "D-pad right")
  @runtimeProperty("ModSettings.displayValues.DpadDown", "D-pad down")
  @runtimeProperty("ModSettings.displayValues.DpadLeft", "D-pad left")
  let btnMenuUp: GWheelButton = GWheelButton.None;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Buttons: menus")
  @runtimeProperty("ModSettings.category.order", "440")
  @runtimeProperty("ModSettings.displayName", "Menu down")
  @runtimeProperty("ModSettings.displayValues.None", "Not bound")
  @runtimeProperty("ModSettings.displayValues.Cross", "Cross (X)")
  @runtimeProperty("ModSettings.displayValues.Square", "Square")
  @runtimeProperty("ModSettings.displayValues.Circle", "Circle")
  @runtimeProperty("ModSettings.displayValues.Triangle", "Triangle")
  @runtimeProperty("ModSettings.displayValues.PaddleRight", "Right paddle")
  @runtimeProperty("ModSettings.displayValues.PaddleLeft", "Left paddle")
  @runtimeProperty("ModSettings.displayValues.R2", "R2")
  @runtimeProperty("ModSettings.displayValues.L2", "L2")
  @runtimeProperty("ModSettings.displayValues.Share", "Share")
  @runtimeProperty("ModSettings.displayValues.Options", "Options")
  @runtimeProperty("ModSettings.displayValues.R3", "R3")
  @runtimeProperty("ModSettings.displayValues.L3", "L3")
  @runtimeProperty("ModSettings.displayValues.Plus", "Plus (+)")
  @runtimeProperty("ModSettings.displayValues.Minus", "Minus (-)")
  @runtimeProperty("ModSettings.displayValues.DialRight", "Red dial right")
  @runtimeProperty("ModSettings.displayValues.DialLeft", "Red dial left")
  @runtimeProperty("ModSettings.displayValues.DialPress", "Red dial press")
  @runtimeProperty("ModSettings.displayValues.PS", "PS button")
  @runtimeProperty("ModSettings.displayValues.DpadUp", "D-pad up")
  @runtimeProperty("ModSettings.displayValues.DpadRight", "D-pad right")
  @runtimeProperty("ModSettings.displayValues.DpadDown", "D-pad down")
  @runtimeProperty("ModSettings.displayValues.DpadLeft", "D-pad left")
  let btnMenuDown: GWheelButton = GWheelButton.None;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Buttons: menus")
  @runtimeProperty("ModSettings.category.order", "440")
  @runtimeProperty("ModSettings.displayName", "Menu left")
  @runtimeProperty("ModSettings.displayValues.None", "Not bound")
  @runtimeProperty("ModSettings.displayValues.Cross", "Cross (X)")
  @runtimeProperty("ModSettings.displayValues.Square", "Square")
  @runtimeProperty("ModSettings.displayValues.Circle", "Circle")
  @runtimeProperty("ModSettings.displayValues.Triangle", "Triangle")
  @runtimeProperty("ModSettings.displayValues.PaddleRight", "Right paddle")
  @runtimeProperty("ModSettings.displayValues.PaddleLeft", "Left paddle")
  @runtimeProperty("ModSettings.displayValues.R2", "R2")
  @runtimeProperty("ModSettings.displayValues.L2", "L2")
  @runtimeProperty("ModSettings.displayValues.Share", "Share")
  @runtimeProperty("ModSettings.displayValues.Options", "Options")
  @runtimeProperty("ModSettings.displayValues.R3", "R3")
  @runtimeProperty("ModSettings.displayValues.L3", "L3")
  @runtimeProperty("ModSettings.displayValues.Plus", "Plus (+)")
  @runtimeProperty("ModSettings.displayValues.Minus", "Minus (-)")
  @runtimeProperty("ModSettings.displayValues.DialRight", "Red dial right")
  @runtimeProperty("ModSettings.displayValues.DialLeft", "Red dial left")
  @runtimeProperty("ModSettings.displayValues.DialPress", "Red dial press")
  @runtimeProperty("ModSettings.displayValues.PS", "PS button")
  @runtimeProperty("ModSettings.displayValues.DpadUp", "D-pad up")
  @runtimeProperty("ModSettings.displayValues.DpadRight", "D-pad right")
  @runtimeProperty("ModSettings.displayValues.DpadDown", "D-pad down")
  @runtimeProperty("ModSettings.displayValues.DpadLeft", "D-pad left")
  let btnMenuLeft: GWheelButton = GWheelButton.None;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Buttons: menus")
  @runtimeProperty("ModSettings.category.order", "440")
  @runtimeProperty("ModSettings.displayName", "Menu right")
  @runtimeProperty("ModSettings.displayValues.None", "Not bound")
  @runtimeProperty("ModSettings.displayValues.Cross", "Cross (X)")
  @runtimeProperty("ModSettings.displayValues.Square", "Square")
  @runtimeProperty("ModSettings.displayValues.Circle", "Circle")
  @runtimeProperty("ModSettings.displayValues.Triangle", "Triangle")
  @runtimeProperty("ModSettings.displayValues.PaddleRight", "Right paddle")
  @runtimeProperty("ModSettings.displayValues.PaddleLeft", "Left paddle")
  @runtimeProperty("ModSettings.displayValues.R2", "R2")
  @runtimeProperty("ModSettings.displayValues.L2", "L2")
  @runtimeProperty("ModSettings.displayValues.Share", "Share")
  @runtimeProperty("ModSettings.displayValues.Options", "Options")
  @runtimeProperty("ModSettings.displayValues.R3", "R3")
  @runtimeProperty("ModSettings.displayValues.L3", "L3")
  @runtimeProperty("ModSettings.displayValues.Plus", "Plus (+)")
  @runtimeProperty("ModSettings.displayValues.Minus", "Minus (-)")
  @runtimeProperty("ModSettings.displayValues.DialRight", "Red dial right")
  @runtimeProperty("ModSettings.displayValues.DialLeft", "Red dial left")
  @runtimeProperty("ModSettings.displayValues.DialPress", "Red dial press")
  @runtimeProperty("ModSettings.displayValues.PS", "PS button")
  @runtimeProperty("ModSettings.displayValues.DpadUp", "D-pad up")
  @runtimeProperty("ModSettings.displayValues.DpadRight", "D-pad right")
  @runtimeProperty("ModSettings.displayValues.DpadDown", "D-pad down")
  @runtimeProperty("ModSettings.displayValues.DpadLeft", "D-pad left")
  let btnMenuRight: GWheelButton = GWheelButton.None;

  // ---- Startup ------------------------------------------------------------

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Startup")
  @runtimeProperty("ModSettings.category.order", "1300")
  @runtimeProperty("ModSettings.displayName", "Ponpon Shit greeting")
  @runtimeProperty("ModSettings.description", "Make the wheel dance when the game starts.")
  let handshakePlayOnStart: Bool = false;

  // ---- Debug --------------------------------------------------------------

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Debug")
  @runtimeProperty("ModSettings.category.order", "1400")
  @runtimeProperty("ModSettings.displayName", "Debug logging")
  @runtimeProperty("ModSettings.description", "Logs to red4ext/logs/gwheel-*.log.")
  let ffbDebugLogging: Bool = false;

  // Map the enum value to the lowercase string token the C++ config
  // layer expects ("default" / "subdued" / "sharp"). Static so OnModSettingsChange
  // doesn't pay the per-call instance cost.
  public static func ResponseCurveToken(c: GWheelResponseCurve) -> String {
    if Equals(c, GWheelResponseCurve.Subdued) { return "subdued"; }
    if Equals(c, GWheelResponseCurve.Sharp)   { return "sharp"; }
    return "default";
  }

  // Map a button dropdown value to the tagged token the plugin's
  // input_bindings layer parses ("wheel:N" / "wheel-pov:DEG").
  public static func ButtonToken(b: GWheelButton) -> String {
    if Equals(b, GWheelButton.Cross) { return "wheel:0"; }
    if Equals(b, GWheelButton.Square) { return "wheel:1"; }
    if Equals(b, GWheelButton.Circle) { return "wheel:2"; }
    if Equals(b, GWheelButton.Triangle) { return "wheel:3"; }
    if Equals(b, GWheelButton.PaddleRight) { return "wheel:4"; }
    if Equals(b, GWheelButton.PaddleLeft) { return "wheel:5"; }
    if Equals(b, GWheelButton.R2) { return "wheel:6"; }
    if Equals(b, GWheelButton.L2) { return "wheel:7"; }
    if Equals(b, GWheelButton.Share) { return "wheel:8"; }
    if Equals(b, GWheelButton.Options) { return "wheel:9"; }
    if Equals(b, GWheelButton.R3) { return "wheel:10"; }
    if Equals(b, GWheelButton.L3) { return "wheel:11"; }
    if Equals(b, GWheelButton.Plus) { return "wheel:19"; }
    if Equals(b, GWheelButton.Minus) { return "wheel:20"; }
    if Equals(b, GWheelButton.DialRight) { return "wheel:21"; }
    if Equals(b, GWheelButton.DialLeft) { return "wheel:22"; }
    if Equals(b, GWheelButton.DialPress) { return "wheel:23"; }
    if Equals(b, GWheelButton.PS) { return "wheel:24"; }
    if Equals(b, GWheelButton.DpadUp) { return "wheel-pov:0"; }
    if Equals(b, GWheelButton.DpadRight) { return "wheel-pov:9000"; }
    if Equals(b, GWheelButton.DpadDown) { return "wheel-pov:18000"; }
    if Equals(b, GWheelButton.DpadLeft) { return "wheel-pov:27000"; }
    return "";
  }

  // ---- Listener callbacks (invoked by Mod Settings, NOT cb funcs) --------

  public func OnModSettingsChange() -> Void {
    this.Push();
  }

  public func Push() -> Void {
    GWheel_SetInputEnabled(this.inputEnabled);
    GWheel_SetSteerDeadzonePct(this.steerDeadzonePct);
    GWheel_SetSteerGainPct(this.steerGainPct);
    GWheel_SetSteerSpeedPct(this.steerSpeedPct);
    GWheel_SetResponseCurve(GWheelSettings.ResponseCurveToken(this.responseCurve));

    GWheel_SetFfbEnabled(this.ffbEnabled);
    GWheel_SetFfbDebugLogging(this.ffbDebugLogging);
    GWheel_SetFfbTorquePct(this.ffbTorquePct);
    GWheel_SetFfbWheelRangeDeg(this.ffbWheelRangeDeg);

    GWheel_SetStationaryThresholdMps(this.stationaryThresholdMps);
    GWheel_SetYawFeedbackPct(this.yawFeedbackPct);
    GWheel_SetActiveTorqueStrengthPct(this.activeTorqueStrengthPct);
    GWheel_SetFfbInvertDirection(this.ffbInvertDirection);
    GWheel_SetFfbOutputSignFlip(this.ffbOutputSignFlip);
    GWheel_SetFfbUseSpring(this.ffbUseSpring);

    GWheel_SetHandshakePlayOnStart(this.handshakePlayOnStart);

    GWheel_SetLedEnabled(this.ledEnabled);
    GWheel_SetLedVisualizerWhileMusic(this.ledVisualizerWhileMusic);

    // Action bindings. Names must match the keys gwheel::axis_resolver
    // and the future button-action dispatcher look up. Values are
    // pipe-delimited tagged-token strings emitted by the InputBinding
    // primitive — empty string means unbound, which the C++ side treats
    // as "no axis source" (zero) for axes and "no dispatch" for buttons.
    GWheel_SetActionBinding("SteerAxis",          this.bindSteerAxis);
    GWheel_SetActionBinding("ThrottleAxis",       this.bindThrottleAxis);
    GWheel_SetActionBinding("BrakeAxis",          this.bindBrakeAxis);

    GWheel_SetActionBinding("Horn",               GWheelSettings.ButtonToken(this.btnHorn));
    GWheel_SetActionBinding("Headlights",         GWheelSettings.ButtonToken(this.btnHeadlights));
    GWheel_SetActionBinding("Handbrake",          GWheelSettings.ButtonToken(this.btnHandbrake));
    GWheel_SetActionBinding("Autodrive",          GWheelSettings.ButtonToken(this.btnAutodrive));
    GWheel_SetActionBinding("ExitVehicle",        GWheelSettings.ButtonToken(this.btnExitVehicle));
    GWheel_SetActionBinding("CallVehicle",        GWheelSettings.ButtonToken(this.btnCallVehicle));

    GWheel_SetActionBinding("CameraCycleForward", GWheelSettings.ButtonToken(this.btnCameraCycleForward));
    GWheel_SetActionBinding("RearViewCamera",     GWheelSettings.ButtonToken(this.btnRearViewCamera));
    GWheel_SetActionBinding("CameraReset",        GWheelSettings.ButtonToken(this.btnCameraReset));

    GWheel_SetActionBinding("ShootPrimary",       GWheelSettings.ButtonToken(this.btnShootPrimary));
    GWheel_SetActionBinding("ShootSecondary",     GWheelSettings.ButtonToken(this.btnShootSecondary));
    GWheel_SetActionBinding("ShootTertiary",      GWheelSettings.ButtonToken(this.btnShootTertiary));

    GWheel_SetActionBinding("NextWeapon",         GWheelSettings.ButtonToken(this.btnNextWeapon));
    GWheel_SetActionBinding("PrevWeapon",         GWheelSettings.ButtonToken(this.btnPrevWeapon));
    GWheel_SetActionBinding("WeaponSlot1",        GWheelSettings.ButtonToken(this.btnWeaponSlot1));
    GWheel_SetActionBinding("WeaponSlot2",        GWheelSettings.ButtonToken(this.btnWeaponSlot2));
    GWheel_SetActionBinding("SwitchWeapons",      GWheelSettings.ButtonToken(this.btnSwitchWeapons));
    GWheel_SetActionBinding("HolsterWeapon",      GWheelSettings.ButtonToken(this.btnHolsterWeapon));

    GWheel_SetActionBinding("RadioMenu",          GWheelSettings.ButtonToken(this.btnRadioMenu));
    GWheel_SetActionBinding("RadioNext",          GWheelSettings.ButtonToken(this.btnRadioNext));

    GWheel_SetActionBinding("UseConsumable",      GWheelSettings.ButtonToken(this.btnUseConsumable));
    GWheel_SetActionBinding("IconicCyberware",    GWheelSettings.ButtonToken(this.btnIconicCyberware));
    GWheel_SetActionBinding("QuickSave",          GWheelSettings.ButtonToken(this.btnQuickSave));

    GWheel_SetActionBinding("OpenMap",            GWheelSettings.ButtonToken(this.btnOpenMap));
    GWheel_SetActionBinding("OpenJournal",        GWheelSettings.ButtonToken(this.btnOpenJournal));
    GWheel_SetActionBinding("OpenInventory",      GWheelSettings.ButtonToken(this.btnOpenInventory));
    GWheel_SetActionBinding("OpenPhone",          GWheelSettings.ButtonToken(this.btnOpenPhone));
    GWheel_SetActionBinding("OpenPerks",          GWheelSettings.ButtonToken(this.btnOpenPerks));
    GWheel_SetActionBinding("OpenCrafting",       GWheelSettings.ButtonToken(this.btnOpenCrafting));
    GWheel_SetActionBinding("Pause",              GWheelSettings.ButtonToken(this.btnPause));

    GWheel_SetActionBinding("MenuConfirm",        GWheelSettings.ButtonToken(this.btnMenuConfirm));
    GWheel_SetActionBinding("MenuCancel",         GWheelSettings.ButtonToken(this.btnMenuCancel));
    GWheel_SetActionBinding("MenuUp",             GWheelSettings.ButtonToken(this.btnMenuUp));
    GWheel_SetActionBinding("MenuDown",           GWheelSettings.ButtonToken(this.btnMenuDown));
    GWheel_SetActionBinding("MenuLeft",           GWheelSettings.ButtonToken(this.btnMenuLeft));
    GWheel_SetActionBinding("MenuRight",          GWheelSettings.ButtonToken(this.btnMenuRight));
  }
}

// Attach our settings instance to the player puppet so it lives for the
// session and register it as a listener with Mod Settings. The listener
// receives OnModSettingsChange callbacks when the user clicks Apply in the
// settings menu — we use that to push the new values into the C++ plugin
// via the GWheel_Set* natives.
//
// The hidden capability flags (hasFfbHardware / hasRevLeds /
// hasRightCluster) are not seeded here. Mod Settings ignores writes to the
// listener instance for dependency / visibility purposes. Those flags are
// driven directly into mod_settings/user.ini from the C++ side at plugin
// load (see gwheel/src/mod_settings_seed.cpp).

@addField(PlayerPuppet)
public let m_gwheelSettings: ref<GWheelSettings>;

@wrapMethod(PlayerPuppet)
protected cb func OnGameAttached() -> Bool {
  let result: Bool = wrappedMethod();
  if !IsDefined(this.m_gwheelSettings) {
    this.m_gwheelSettings = new GWheelSettings();
    ModSettings.RegisterListenerToClass(this.m_gwheelSettings);
    ModSettings.RegisterListenerToModifications(this.m_gwheelSettings);
    this.m_gwheelSettings.Push();
  }
  return result;
}
