# Match runner contract

`nutmeg-run` is the headless entry point Pitch (the Laravel career and league app) calls from a queued job. It reads one JSON match file and writes one JSON result file. This document is the contract; the machine-readable form is:

- [`runner/schema/match-input.schema.json`](../runner/schema/match-input.schema.json)
- [`runner/schema/match-result.schema.json`](../runner/schema/match-result.schema.json)

**The simulation behind this contract is a stub.** Until the real engine lands, `nutmeg-run` produces a seeded, deterministic, schema-valid result that plays no football (a score generator with plausible counters and events). The result says so with `"stub": true`. Consumers must not treat stub numbers as football. Everything else here (the file shapes, validation, exit codes, determinism) is the real contract.

## Command line

```
nutmeg-run <match.json> [-o <result.json>]
nutmeg-run --version
```

Without `-o` the result goes to stdout. With `-o` the file is written atomically (a temporary sibling, then a rename), and is not touched when the run fails. Problems with the input are written to stderr, one per line, as `nutmeg-run: <kind>: <JSON pointer>: <message>`.

| Exit code | Meaning |
| --- | --- |
| 0 | Ok. The result was written. |
| 1 | Usage error, or the input could not be read or the output could not be written. |
| 2 | Invalid input: not JSON, or fails the input schema. Nothing is written. |
| 3 | Unsupported version: `schemaVersion` or `engineVersion` is not one this runner reads. Nothing is written. |

Version checks run before any other validation, so a file of a future schema version always exits 3 rather than 2, however different its shape.

## Versions

- `schemaVersion` is an integer, currently `1`. It changes when a field is removed, renamed or retyped. Adding optional fields does not change it.
- `engineVersion` is the version Pitch expects to be talking to, as `major.minor.patch`. The runner accepts the file when `major.minor` equals its own and ignores the patch. Pitch should store the engine version a match was played with (it is echoed in the result) and send it back when it wants a replay, because the same seed on a different engine minor version gives a different match.
- The result carries the same two fields, with the engine version actually used.

## Determinism

The same input file produces a byte-identical result file on Windows and Linux. Keys are written in alphabetical order, two-space indent, LF line endings, and a trailing newline. A replay is therefore "run the runner again with the stored match file", which Pitch can use instead of storing per-frame data. `seed` is an integer from 0 to 2^53 - 1, so it survives a round trip through PHP and JavaScript numbers.

## Input

```json
{
  "schemaVersion": 1,
  "engineVersion": "0.1.0",
  "matchId": "fixture-4021",
  "seed": 123456789,
  "context": { "homeAdvantage": 1.06, "weather": { "kind": "clear" } },
  "home": { "name": "...", "formation": "433", "mentality": "attacking", "lineup": [ ... 11 ... ], "bench": [ ... ] },
  "away": { ... }
}
```

A complete example is [`runner/tests/data/valid_match.json`](../runner/tests/data/valid_match.json).

| Field | Required | Notes |
| --- | --- | --- |
| `schemaVersion` | yes | `1`. |
| `engineVersion` | yes | Expected engine, see Versions. |
| `matchId` | no | Opaque string up to 128 bytes, echoed in the result. Pitch can put its fixture or live match id here. |
| `seed` | yes | Pitch's `fixtures.seed`. |
| `context.homeAdvantage` | no | Multiplier on the home side's attack, 0.5 to 1.5, default 1.0. Pitch's `FixtureResolver` uses 1.06 home and 0.96 away; send 1.06. |
| `context.weather.kind` | no | Placeholder. Only `clear` is accepted for now. |
| `home`, `away` | yes | Teams, below. |

### Team

| Field | Required | Notes |
| --- | --- | --- |
| `name` | yes | Display name. |
| `formation` | yes | Pitch's `Formation::$id`: `442`, `433`, `352`, `4231`, `532`, `343`. |
| `mentality` | yes | Pitch's `Mentality` value: `attacking`, `balanced`, `defensive`. |
| `lineup` | yes | Exactly 11 players, exactly one with position `GK`. Pitch lays out ten outfielders by formation slot plus a keeper, so a player's `slot` (1 to 10 for outfielders, 0 for the keeper) says where in the formation they stand. When `slot` is missing the order of the array decides, with the keeper wherever it appears. |
| `bench` | no | Up to 12 players, same shape. The stub makes no substitutions, so bench players do not appear in the result. |

### Player

| Field | Required | Notes |
| --- | --- | --- |
| `id` | yes | Integer of at least 1, unique across both teams. Pitch's player id. The result refers to players by this id only, so Pitch needs no name matching. |
| `name` | yes | Display name. |
| `position` | yes | Pitch's `Position` value: `GK`, `DF`, `MF`, `FW`. |
| `slot` | no | Formation slot, see above. |
| `shirt` | no | Shirt number, 1 to 99. |
| `attributes` | yes | See below. |

### Attributes

The six legacy attributes `vision`, `passing`, `dribbling`, `finishing`, `tackling` and `pace` are required integers from 1 to 100, exactly as in Pitch's `Attributes` class. Any other key is an optional extra, also an integer from 1 to 100; an omitted extra takes its default.

The list of extras, their meaning and their defaults is defined by ticket NUT-25 and documented in `docs/attributes.md` once that lands. This contract deliberately does not repeat it: the runner accepts any extra name with a valid value, and the simulation reads the ones it knows. Pitch should send the legacy six always and extras only when it has real values.

Pitch applies condition factors (fitness, form) with `Attributes::scaled()` before building the match. The runner takes the numbers as given and applies no further scaling, so Pitch sends the effective values.

## Result

A complete example is [`runner/tests/data/expected_result.json`](../runner/tests/data/expected_result.json).

Top level: `schemaVersion`, `engineVersion` (used), `stub` (true while the simulation is a stub), `matchId` (echoed when given), `seed` (echoed), `score`, `teams`, `players`, `events`.

### `score`

`{ "home": int, "away": int }`. This replaces `Fixture::home_goals` and `away_goals`.

### `teams.home` and `teams.away`

Field names follow Pitch's `MatchSummary::forDisplay()` so the mapping is a rename of nothing:

| Field | Notes |
| --- | --- |
| `possession` | Percent with one decimal. The two sides add up to 100. |
| `shots` | Includes goals. |
| `onTarget` | Goals plus saved shots. Pitch's `onTarget`. |
| `goals` | Equals the score. |
| `passes`, `passesCompleted` | Pitch derives `passAccuracy` from these. |
| `crosses`, `fouls`, `corners`, `tackles`, `saves` | As in `MatchSummary`. |
| `offsides`, `yellowCards`, `redCards` | New; Pitch does not track them yet. |

### `players`

One entry per player who appeared, never for unused bench players. Fields follow `MatchSummary`'s per-player counters, with `minutes` in place of Pitch's `ticks`:

`id`, `team` (`home` or `away`), `minutes`, `rating` (1.0 to 10.0, one decimal, the figure `MatchRatings` produces), `goals`, `shots`, `passes`, `passesCompleted`, `crosses`, `tackles`, `interceptions`, `clearances`, `fouls`, `saves`.

### `events`

Ordered by `minute`, ties in the order the engine produced them. Each event has:

| Field | Notes |
| --- | --- |
| `minute` | Integer, 0 to 130 (stoppage time included). |
| `type` | Pitch's `EventType` values, plus `offside`: `shot`, `goal`, `save`, `block`, `foul`, `corner`, `offside`, `yellow_card`, `red_card`, `pass`, `cross`, `dribble`, `turnover`, `interception`, `tackle`, `slide_tackle`, `clearance`, `header`, `throw_in`, `goal_kick`, `penalty`. The stub emits only `shot`, `goal`, `save`, `foul`, `yellow_card`, `corner` and `offside`; the real engine may emit any. Consumers must ignore types they do not know. |
| `team` | `home` or `away`: the side the acting player belongs to. A `save` belongs to the keeper's side. |
| `playerId` | The acting player's `id`: the shooter, scorer, fouler, carded player, corner taker, offside player, or saving keeper. |
| `otherPlayerId` | Optional second player: the assist on a `goal`, the fouled player on a `foul`, the shooter on a `save`. |
| `outcome` | Optional, on `shot` and `goal`: `goal`, `saved`, `blocked` or `off`. This is the classification `MatchSummary::shot()` computes from the following event, so `shots` rows can be built without lookahead. |
| `x`, `y` | Pitch position normalised to 0..1, in the same frame as Pitch's `MatchLineups`: `x` runs from the home goal line (0) to the away goal line (1), so the home side attacks towards `x = 1`; `y` runs across the pitch. |

Goals are `goal` events, not a `shot` plus a `goal`; a scored attempt appears once, with `outcome: "goal"`.

## How Pitch maps this

| Pitch | From the result |
| --- | --- |
| `Fixture::home_goals`, `away_goals` | `score.home`, `score.away` |
| `MatchSummary` team counters | `teams.home` and `teams.away` (same names) |
| `MatchSummary` player counters, `MatchRatings` | `players[]` (same names, `minutes` for `ticks`) |
| `MatchSummary` shot list | `events[]` where `type` is `shot` or `goal`: `minute`, `team`, `playerId`, `x`, `y`, `outcome` |
| Moments and commentary (`MatchMoment`, `MatchNarrator`) | Built by Pitch from `events[]`; the runner writes no prose |
| `MatchReplayController` frames and 2D `positions` | Not in this version. Per-frame player and ball positions will be a later, separate output so the result stays small; the seed and input are enough to reproduce them. |

## Changing the contract

Add optional fields freely and note them here. Anything that removes, renames or retypes a field bumps `schemaVersion` and the runner keeps exiting 3 for versions it does not read. Update the schema files, this document and the golden files in `runner/tests/data/` together; the tests check that the example input and result conform to the schemas.
