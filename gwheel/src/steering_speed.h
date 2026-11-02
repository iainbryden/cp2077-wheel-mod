#pragma once

#include <cstdint>

// Steering speed — scales how fast the game lets a vehicle's front wheels
// turn toward the requested steer angle.
//
// Every drive-model TweakDB record (cars, bikes) carries
// wheelTurnMaxAddPerSecond / wheelTurnMaxSubPerSecond: the maximum rate, in
// degrees per second, at which the road wheels move toward the input (Add)
// and back toward center (Sub). Authored values are ~95-130 deg/s, tuned
// for binary keyboard input; on a wheel they read as input lag, because the
// tyres keep catching up after the driver's hands have stopped.
//
// Apply() rewrites both flats on every drive-model record to
// original * pct / 100 and refreshes the records. The game copies these
// values when a vehicle spawns, so a change affects vehicles spawned after
// the call (re-summon the car or reload a save to pick it up), not the one
// currently driven.
//
// Originals are captured on first use and restored exactly at 100%, so 100
// leaves TweakDB untouched in effect. Changes live in memory only; nothing
// is written to game files, and a restart resets everything.
//
// Safeguards, since TweakDB writes are the riskiest thing this plugin does:
//   - Every candidate flat is validated (exists, is a Float, holds a
//     plausible rate) before anything is written; if more than 10% fail,
//     nothing is written this session.
//   - Written rates are capped at 3000 deg/s.
//   - A marker file (steering_speed.applying, next to the DLL) exists only
//     while writes are in progress. If a session dies mid-write, the next
//     session skips the writes once and logs why, instead of repeating it.
//   - At 100% with nothing changed this session, TweakDB is never touched.
//
// Must be called from the game / script thread (it is driven by the
// GWheel_SetSteerSpeedPct native), never from the pump or other worker
// threads: TweakDB flat creation is not safe to race with itself.
namespace gwheel::steering_speed
{
    // Apply the multiplier. Returns false if TweakDB isn't ready yet or no
    // records were found; the next call retries. Calling again with the
    // value already applied is a no-op.
    bool Apply(int32_t pct);
}
