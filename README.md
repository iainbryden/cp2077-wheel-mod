# NightDrive: Logitech G-series Steering Wheel for Cyberpunk 2077

**v2.5.0-nightdrive.8** for Cyberpunk 2077 game patch 2.5.

> **NightDrive** is a fork of [cp2077-wheel-mod](https://github.com/clevergrant/cp2077-wheel-mod) by Grant Perdue, updated for game patch 2.5 by Iain Bryden. See [Credits](#credits).

Drive Cyberpunk 2077 with a Logitech G-series steering wheel. Your wheel handles steering, throttle, and brake. Force feedback fires from in-game physics (centering as you turn, cornering load, surface texture, collision jolts, slip when you lose grip). Rev-strip LEDs follow real engine RPM. When the in-car radio is on, the rev strip becomes a music visualizer.

There is no virtual gamepad to install, no XInput shim, no third-party driver. The wheel talks to the game via the official Logitech Steering Wheel SDK (the same one driving sims use).

## What's new in NightDrive

- Works on game patch 2.5.
- Works with the regular Mod Settings mod. No replacement files, no Vortex file conflicts.
- Wheel buttons are bound from dropdown menus in Mod Settings.
- New settings: **Steering sensitivity**, **Steering speed** (how fast the road wheels follow your hands) and **Wheel rotation** (scales force feedback to your G HUB range).
- Lower input latency.
- Held buttons are always released: getting out of the car, alt-tabbing or unplugging the wheel can't leave a key stuck down. Wheel buttons only reach the game, never another window you switch to.

## Supported wheels

Tested on a **G29** (patch 2.5, G HUB).

The original mod was tested on the G923 (Xbox) and G920, and is designed for every Logitech wheel G HUB supports:

- WingMan Formula Force, Formula Force GP
- Driving Force, Driving Force Pro, Driving Force GT
- Momo Force, Momo Racing
- G25, G27, G29, G920
- G923 (Xbox, PS, PC variants)
- G PRO Racing Wheel
- Formula Vibration Feedback

The mod auto-detects what hardware your wheel has (force feedback motor, rev-strip LEDs, right-hand button cluster) and only shows the relevant settings.

Non-Logitech wheels (Thrustmaster, Fanatec, Moza) are not supported.

## Requirements

- Cyberpunk 2077 **patch 2.5**
- **[RED4ext](https://www.nexusmods.com/cyberpunk2077/mods/2380)** 1.31.0 or later
- **[redscript](https://www.nexusmods.com/cyberpunk2077/mods/1511)** 0.5.32 or later
- **[ArchiveXL](https://www.nexusmods.com/cyberpunk2077/mods/4198)**
- **[Mod Settings](https://www.nexusmods.com/cyberpunk2077/mods/4885)** 0.2.28 or later
- **[Logitech G HUB](https://www.logitech.com/innovation/g-hub.html)**, running

## Install

**Vortex:** install the requirements above, then install this mod from its archive and deploy.

**Manual:** extract the archive into the Cyberpunk 2077 game folder. It adds:

- `red4ext\plugins\gwheel\` (the plugin, its `config.json` and the license)
- `r6\scripts\gwheel\` (the menu and game scripts)

**Upgrading from the original mod (v2.31.x):** remove it first, and let Vortex restore the regular Mod Settings files it replaced.

## Set up your wheel

1. **Steering and pedals work right away.** They're preset to the standard Logitech DirectInput layout: steering on the X axis, throttle on Y, brake on Rz.
2. **Buttons:** open **Main Menu → Mod Settings → G-series Wheel** and pick a wheel button for each action you want, under **Buttons: driving / camera / combat / radio and items / menus**. Every action starts unbound.

The button names in the dropdowns (Cross, Square, paddles, D-pad, dial and so on) follow the G29's layout. If a button does something different on your wheel, see Troubleshooting.

## G HUB

G HUB and this mod run at the same time. G HUB keeps the operating range, sensitivity and centering spring; the mod adds the game's force feedback on top.

Recommended start: G HUB **operating range 180°**, so the wheel matches the in-car steering animation. Then set the mod's **Wheel rotation** to the same number (see Settings).

If your G HUB Cyberpunk profile binds wheel buttons to keys, clear the ones that overlap with the bindings here, or you'll get doubled presses.

## Settings

All settings live in **Main Menu → Mod Settings → G-series Wheel**.

- **Wheel input.**
  - Master enable.
  - Steering deadzone: filters small jitter at center; 2–3% is typical.
  - Response curve: Default / Subdued / Sharp.
  - **Steering sensitivity (%)**, default 300. How much in-game lock you get per degree of wheel. Higher turns the car more for the same rotation; lower if the car turns too much.
  - **Steering speed (%)**, default 300. How fast the road wheels follow your hands. The game's own limit is tuned for keyboards and feels like lag on a wheel. Applies to vehicles spawned after the change: re-summon your car or reload a save.
- **Force feedback.**
  - Master enable.
  - **FFB strength (%)**, default 40.
  - **Wheel rotation (degrees)**, default 180. Set it to your G HUB operating range. Current G HUB versions don't report the range to games, so the mod can't read it. 0 means try to read it from G HUB, falling back to 540.
  - Cornering feedback, active torque and stationary threshold.
  - **Invert FFB direction:** a Logitech quirk, see Troubleshooting.
  - **Speed-sensitive stiffness:** sportscar-style assist, on or off.
- **Rev-strip LEDs.** Master enable, and the music visualizer while the radio plays.
- **Buttons.** Driving, camera, combat, radio and items, menus.
- **Startup.** Wheel greeting animation when the game starts; off by default.
- **Debug.** Extra logging to `red4ext/logs/gwheel-*.log`.

Sections for hardware your wheel doesn't have are hidden.

## Troubleshooting

- **Wheel detected but the car doesn't move.** Make sure RED4ext, redscript and Mod Settings are installed and current for patch 2.5, and that G HUB is running. Check `red4ext/logs/gwheel-*.log` for errors.
- **Settings page is missing.** Mod Settings or ArchiveXL aren't installed correctly. The wheel still drives with the defaults, but buttons can't be bound.
- **A button does something different from its name in the menu.** Button numbering can differ between wheel models. Press the button in game, then look in `red4ext/logs/gwheel-*.log` for `[gwheel:button] wheel:N pressed`. Pick the dropdown entry with that number.
- **Steering feels laggy: the car keeps turning after your hands stop.** Raise **Steering speed**, then re-summon your car or reload a save.
- **The car turns too much or too little.** Adjust **Steering sensitivity**. G HUB's own sensitivity slider also affects it; 50 is neutral there.
- **Force feedback feels weak.** Set **Wheel rotation** to match your G HUB operating range.
- **Doubled keypresses on a wheel button.** G HUB and this mod are both binding the same key. Clear the entry from your G HUB Cyberpunk profile, or unbind that action here.
- **Wheel rolls away from center instead of recovering, or fights you back into the turn under load.** Enable **Invert FFB direction** in Force feedback. Some Logitech unit, firmware and G HUB combinations apply the SDK's force direction the opposite way to its documentation. Forza, iRacing and most sim titles have the same toggle.
- **Wheel feels too stiff or too floppy at speed.** Toggle **Speed-sensitive stiffness**. On (default): the wheel firms up at speed. Off: pure tire-force feedback, like BeamNG; light at rest and more raw at speed.

## Credits

### Original mod

NightDrive is built on **[cp2077-wheel-mod](https://github.com/clevergrant/cp2077-wheel-mod) by Grant Perdue** (clevergrant). The plugin's design, the force-feedback model, the LED and music visualizer, the button dispatch, the TrueForce work and the Mod Settings integration are his. NightDrive is an independent fork; Grant Perdue does not maintain or endorse it. Please report NightDrive problems to the NightDrive fork, not to him.

Licensed under the MIT License. The original copyright notice is kept in [LICENSE](LICENSE).

### NightDrive changes

By Iain Bryden ([iainbryden](https://github.com/iainbryden)):

- Game patch 2.5 support (RED4ext.SDK update, builds with older Windows SDKs)
- Works with upstream Mod Settings 0.2.28; button bindings are menu dropdowns
- Steering sensitivity setting
- Steering speed setting (faster road-wheel response)
- Wheel rotation setting for force-feedback scaling
- Input latency fixes (steady 4 ms pump, non-blocking taps, faster FFB updates)
- Held-input fixes: keys are always released, input only goes to the game, settings reads are thread-safe
- Unit tests and GitHub Actions CI

### Frameworks and libraries

- [RED4ext](https://github.com/WopsS/RED4ext) and [RED4ext.SDK](https://github.com/WopsS/RED4ext.SDK) by WopsS
- [redscript](https://github.com/jac3km4/redscript) by jac3km4
- [Mod Settings](https://github.com/jackhumbert/mod_settings) by jackhumbert
- [ArchiveXL](https://github.com/psiberx/cp2077-archive-xl) by psiberx
- Logitech Steering Wheel SDK, © Logitech Inc. Used under Logitech's own terms, not the MIT License. Logitech, G HUB and the wheel model names are trademarks of Logitech. This project is not affiliated with or endorsed by Logitech or CD PROJEKT RED.

## Reporting issues

Report NightDrive problems on the NightDrive Nexus page (Posts or Bugs tab). Source code: [github.com/iainbryden/cp2077-wheel-mod](https://github.com/iainbryden/cp2077-wheel-mod). Include your wheel model, Cyberpunk patch version, RED4ext version, and your most recent `red4ext/logs/gwheel-*.log` file.
