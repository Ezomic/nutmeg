# 0001: Nutmeg is a real-time C++ engine, separate from Pitch

Status: accepted, 2026-10-26. Ticket: NUT-7.

## Context

Pitch is a Laravel football manager. Its match engine is deterministic PHP that
runs a 2,700 tick loop at 5 ticks per second on a 0..1 normalised pitch, and the
manager decides while the engine plays. We want a FIFA/PES-style game instead:
real time, a human on a controller, open source.

PHP cannot be that engine. Ichor, a sibling project, already runs on its own
C++20 engine with rules that fit a sports game almost word for word: fixed
timestep, input as a recorded stream, one seeded RNG per system, null backends
so everything runs headless.

## Decisions

1. **Nutmeg is a new repository and a new engine.** It is not built inside
   Ichor or inside Pitch.
2. **Ichor code is copied once, not shared.** The generic parts of Ichor's
   `core` and `platform` (math, hashing, RNG, fixed step, logging, atomic file
   writes, binary encoding, the null platform) were copied and renamed to the
   `nutmeg` namespace. Nothing is kept in sync. Ichor's world, ledger, faction
   and Lua code is not taken.
3. **Pitch stays the career and league layer.** It sends lineups, attributes,
   formations, mentality and a seed to a headless Nutmeg runner and receives a
   result, stats and an event log. Nutmeg never renders for Pitch.
4. **The PHP positional engine is frozen.** No new engine features go into PHP.
   The unbuilt ones are specced in this project instead.
5. **Simulation model:** a fixed 60Hz step, positions in metres on a 105 by
   68 metre pitch, rendering interpolated between steps.
6. **Determinism:** exact on the same build and platform. Replay files carry the
   engine version. Ichor's `dmath` (sin, cos and atan2 from basic arithmetic)
   and `-ffp-contract=off` on gcc and clang make bit-identical results across
   Windows and Linux plausible, so cross-platform replay is a goal we test for,
   not a promise.
7. **Physics:** an analytic ball (gravity, drag, spin, bounce) and simple player
   collision. No Jolt until it is needed.
8. **Controls:** gamepad first, one controlled player at a time, auto switch to
   the player nearest the ball, keyboard as a fallback.
9. **Camera and rendering:** the target is a broadcast side-on 3D camera. The
   first milestone is a plain SDL3 top-down view with capsule players. Ichor's
   Vulkan renderer is brought over only when 3D starts.
10. **First milestone:** 5-a-side on a flat pitch, one human against AI, goals
    and out of play only. No referee, offside, cards or set pieces yet.
11. **Runner interface:** a CLI reading a versioned JSON match file and writing a
    JSON result and event file. Pitch calls it from a queued job.
12. **Platforms and CI:** developed on Windows, with a single Linux CI job that
    runs the headless suite. There is no CI matrix. Windows is verified locally
    before a PR.
13. **Licences:** Apache-2.0 for the code copied from Ichor, GPL-3.0 for the
    game, CC0 or CC BY-SA for data. Data is fictional by default. Community
    packs may carry real names, and trademark responsibility is the pack
    author's.

## Out of scope for now

- Online play, until after the first playable milestone.
- Career, league and transfer logic, which stays in Pitch.
- Referee rules, set pieces, animated models and audio in milestone 1.

## Consequences

- The old PHP golden master cannot be reused. Realism is re-established with new
  bands and a new golden master blessed on purpose (NUT-21).
- Ichor and Nutmeg will drift where they overlap. That is accepted: the copied
  files are small and stable.
- Pitch's league resolution (PITCH-167) becomes a runner call (NUT-33) rather
  than a faster PHP engine.
