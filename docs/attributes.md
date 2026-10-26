# Player attributes

The C++ types live in `schema/` (`nutmeg/schema/attributes.h`, `nutmeg/schema/player.h`). The match-file JSON schema (NUT-33) uses the serialised names below.

## Rules

- Every rating is an integer on the **1..100** scale. Out-of-range values are clamped, never rejected.
- Only the six Pitch attributes plus `handling` ever come from Pitch. Every other field is optional. A missing field takes its default, so a record with just the legacy fields is valid.
- Defaults are a neutral **50**. Height, weight and foot have their own defaults below.
- Serialised names are `snake_case`. Unknown attribute names are an error for the parser to report (`Partial::set` returns false).
- `scaled(factor)` multiplies every rating by `factor`, rounds half away from zero and clamps to 1..100. `plus(name, delta)` adds `delta` to one named rating and clamps. Both match `Attributes::scaled()` and `Attributes::plus()` in Pitch, and apply to the extras too.
- `overall()` is the rounded mean of the six legacy attributes only, as in Pitch.
- Binary encoding (`core/binary.h`) is each field in table order, `int32` per rating.

## Outfield attributes (`Attributes`)

| Field | Range | Default | Origin | Used by |
| --- | --- | --- | --- | --- |
| `vision` | 1..100 | 50 | Pitch | AI pass and run target selection (how far and how well a player reads the pitch) |
| `passing` | 1..100 | 50 | Pitch | Pass accuracy and weight (error added to the intended pass vector) |
| `dribbling` | 1..100 | 50 | Pitch | Ball control while carrying, turning speed with the ball, beating a tackle |
| `finishing` | 1..100 | 50 | Pitch | Shot accuracy and power error |
| `tackling` | 1..100 | 50 | Pitch | Tackle success and range, interception chance |
| `pace` | 1..100 | 50 | Pitch | Top speed and acceleration |
| `strength` | 1..100 | 50 | new | Shielding, shoulder challenges, winning contested balls |
| `stamina` | 1..100 | 50 | new | Sprint energy drain and recovery rate |
| `aggression` | 1..100 | 50 | new | AI willingness to press and tackle, foul and card likelihood |
| `crossing` | 1..100 | 50 | new | Cross accuracy and curve |
| `first_touch` | 1..100 | 50 | new | Ball reception quality on a pass or loose ball |
| `composure` | 1..100 | 50 | new | Accuracy under pressure, shot and pass error when marked |
| `jumping` | 1..100 | 50 | new | Aerial duels, headers, reach |

## Goalkeeper attributes (`GoalkeeperAttributes`)

Always present on a profile, read by the engine only when `position` is `GK`. Pitch's `handling` column (default 50) maps straight to `handling`.

| Field | Range | Default | Origin | Used by |
| --- | --- | --- | --- | --- |
| `reflexes` | 1..100 | 50 | new | Reaction time to a shot, close-range saves |
| `handling` | 1..100 | 50 | Pitch | Catch versus parry, rebound size |
| `positioning` | 1..100 | 50 | new | Starting position, angle closing, sweeping |
| `diving` | 1..100 | 50 | new | Reach and speed of a dive |
| `kicking` | 1..100 | 50 | new | Goal kick and throw distance and accuracy |

## Profile (`PlayerProfile`)

| Field | Type | Range | Default | Used by |
| --- | --- | --- | --- | --- |
| `id` | uint32 | any | 0 | Stable player identity in match events and Pitch round trips |
| `name` | string | any | empty | Display and logs |
| `position` | `GK` `DF` `MF` `FW` | one of four | `MF` | Formation slot, default AI role, which attribute set counts |
| `attributes` | `Attributes` | see above | all defaults | All outfield systems |
| `goalkeeper` | `GoalkeeperAttributes` | see above | all defaults | Goalkeeper AI |
| `foot` | `right` `left` `both` | one of three | `right` | Which foot strikes the ball, off-foot accuracy penalty |
| `height_cm` | int | 150..210 | 180 | Collision body height, aerial reach with `jumping` |
| `weight_kg` | int | 50..110 | 75 | Body mass in collisions with `strength` |

`id`, `name` and `position` have no meaningful default; the runner schema should require them. `PlayerProfile::normalised()` clamps all ratings, height and weight.

## Adding a field

Add the member, one row in `AttributeTable` (name, member, default) and a row here. Scaling, clamping, defaults and binary encoding follow from the table. New fields go at the end of their table so existing binary encodings stay readable by position.
