# Realism bands and regression suite

This is the specification for Nutmeg's realism harness (NUT-21). It inherits the harness that the Pitch PHP match engine grew over PITCH-116 to PITCH-175, records what it measured and why, and says what the C++ runner has to emit so the same checks can run. The machine-readable bands are in [data/realism_bands.json](../data/realism_bands.json). No harness code exists yet: the C++ simulation (physics, loop, attributes) is still being built, so nothing here can be measured today.

Source material, all in the Pitch repository: `app/Sim/Analysis/` (`RealismReport`, `RealismHarness`, `MatchAnalyzer`, `MatchMetrics`, `RealismBaseline`, `SideMetrics`, `ExperimentHarness`, `LeagueHarness`, `LeagueReport`, `LeagueMetrics`, `ShapeHarness`, `ShapeResult`, `GoldenMaster`), the `pitch:*` console commands, `tests/Unit/Sim/RealismBandTest.php`, `tests/Unit/Sim/LeagueBandTest.php`, `tests/Unit/Analysis/*`, and tickets PITCH-116, 120, 124, 126, 127, 144, 145, 152, 155 and 175.

## How the PHP harness worked

1. `RealismHarness` plays seeds 1 to N between two evenly rated sides (rating 72, all six attributes equal) and gets a `PitchResult`: score, an event list, and one frame per tick.
2. `MatchAnalyzer` turns each result into a `MatchMetrics` of raw counts. Counts are plain totals so batches aggregate by addition. Rates and ratios are derived later, never stored.
3. `RealismReport` turns a summed batch into ten rows, each with a value, a low and high band, and an `ok` flag.
4. `RealismBandTest` (the ratchet) fails the suite when a metric leaves its band, with the known-off exceptions described below.

Determinism is the property that makes this work: seeds 1 to N always give the same totals, so a run is exact rather than sampled and a test cannot flap on the same code.

## Match metrics

All counts are for both sides combined, per match, unless a unit says percent. Percent metrics are pooled over the batch (sum of numerators over sum of denominators), not averaged per match.

| id | Metric | Band | Unit |
|---|---|---|---|
| `goals_per_match` | Goals | 2.4 to 3.2 | per match |
| `shots_per_match` | Shots | 22 to 30 | per match |
| `shots_on_target_per_match` | Shots on target | 7.5 to 11 | per match |
| `shot_conversion_pct` | Shot conversion | 9 to 14 | % |
| `pass_completion_pct` | Pass completion | 70 to 88 | % |
| `crosses_per_match` | Crosses | 14 to 42 | per match |
| `fouls_per_match` | Fouls | 18 to 28 | per match |
| `corners_per_match` | Corners | 8 to 12 | per match |
| `home_possession_pct` | Home possession | 45 to 55 | % |
| `final_third_time_pct` | Final-third time | 12 to 30 | % |

### Definitions, computation and why each matters

- **Goals.** Home goals plus away goals from the final score. Headline outcome. Too high and every match is a shootout, too low and matches are dull and draws balloon (PITCH-155 saw draws rise with low scoring).
- **Shots.** Every event that is a shot or a header attempt (`EventType::isShot()`). Measures how often play reaches a shooting situation. PITCH-120 found 45.5 against a band of 22 to 30: the engine was "too shot-happy", fixed by shrinking shooting range, not by touching finishing.
- **Shots on target.** Shot events flagged successful (the ball went in) plus `Save` events. A blocked or missed shot is not on target. Shot events carry only "went in or not", so the analyser recovers saves from the following event.
- **Shot conversion.** Goals divided by shots, pooled. Separates "too many shots" from "finishing too good". PITCH-124 trimmed conversion from about 14% to 11.4% so goals fell back into band while shots stayed put.
- **Pass completion.** Completed `Pass` events over all `Pass` events, pooled. Crosses are a separate event type and are excluded. Proxy for build-up quality and defensive pressure.
- **Crosses.** Count of `Cross` events (the analyser also counts completed crosses but no band uses them). Checks that wide play exists. PITCH-152 added `CROSS_WIDTH` and `CROSS_CHANCE` to recover crosses lost when play stopped camping in the final third.
- **Fouls.** Count of `Foul` events. Cards are not part of the figure. Fouls interact with set pieces: more fouls means more direct free kicks, which pushes shots on target back out (PITCH-127), so this one cannot be raised in isolation.
- **Corners.** Count of `Corner` events. Falls with shots: PITCH-152 saw corners go from 9.5 to 5.0 when shots fell from 27 to 18, which the ratchet refused.
- **Home possession.** Per frame, the side in possession is recorded (0 home, 1 away). Metric is home frames over all frames. With evenly rated sides it should sit near half, so it doubles as a symmetry check. A modest home advantage is allowed (PITCH-151), hence 45 to 55.
- **Final-third time.** Per frame, the ball's x is oriented up the possessing side's attacking axis (x for home, 1 minus x for away). Advance above 0.66 counts as final third. Metric is those frames over all frames. Frames below 0.34 count in neither the final nor the middle third. PITCH-152 found this at 40.1% (ball "never coming back") and fixed it by moving the defensive block and the in-possession block, which then needed shooting re-balanced.

### Counted but never banded

`MatchMetrics` also carries throw-ins, goal kicks, penalties, defensive actions (tackles, slide tackles, interceptions, clearances), completed crosses, middle-third frames and a summed shot origin (`shotAdvanceSum`, the pitch-axis position shots were struck from). None has a band. PITCH-116 listed time in thirds and shot-distance and goal-origin distributions as intended outputs. Nutmeg may band these later, but this spec does not invent bands.

## League metrics

PITCH-155 added a second harness for the auto-resolved league, which ran down a different code path (`FixtureResolver`) from the positional engine. `LeagueHarness` plays a round-robin, home and away, over six fixed clubs (ratings 72, 68, 65, 62, 58 and 54, with varied formations and mentalities, players varied around the base rating) over repeated seeds.

| id | Metric | Band | Unit |
|---|---|---|---|
| `league_goals_per_game` | Goals per game | 2.4 to 3.2 | per match |
| `league_home_wins_pct` | Home wins | 40 to 50 | % |
| `league_draws_pct` | Draws | 22 to 30 | % |
| `league_away_wins_pct` | Away wins | 24 to 34 | % |
| `league_goalless_pct` | Goalless | 4 to 14 | % |

These are results-shape checks on full fixtures between unequal sides. PITCH-155 measured 2.6 goals, 46.7% home wins, 23.6% draws and 29.8% away wins. A lesson from that ticket applies directly to Nutmeg: the ticket's premise ("1.8 goals against a real 2.7") came from an ad-hoc set of clubs typed into a scratch file, and acting on it would have broken a healthy engine. The fixed, representative division is the instrument that prevents this, so Nutmeg should define its division once, in data, and never measure against an improvised one.

Nutmeg has a single simulation path, so the separate league harness exists only if a league or season mode later produces unequal sides. Until then these five bands can be checked from the same full-match runner with a fixed varied set of sides, using fixture scores only.

## Where the bands came from

**Every band is given, not tuned.** The ten match bands were written once in PITCH-116 and never changed (`git log` on `RealismReport.php` shows only that commit and the PITCH-155 refactor). The five league bands were written once in PITCH-155. The engine was then calibrated toward the bands, never the reverse. The PHP code describes them as "rough top-division per-match figures (both teams combined)" and "targets to calibrate toward, not hard limits".

The PHP repository cites no external data source for any band. The only per-band reasoning on record is in tickets and commit messages:

- Goals 2.4 to 3.2 and conversion about 11%: PITCH-124 says "real ~3" goals and "real ~11%" conversion.
- League: PITCH-155 says "real top-division 2.7" goals, real draws "26%", and goalless "nearer 8%" (the band is 4 to 14).
- Fouls 18 to 28: PITCH-126 calls it the "fouls target 18-28/match".
- Shots 22 to 30: PITCH-120 says "a real ~26".

Treat the rest (shots on target, pass completion, crosses, corners, possession, final-third time, win/draw split) as the original author's judgement, unsourced. Before Nutmeg freezes these as pass or fail gates, someone should check them against published data (for example public match-statistics datasets) and record the source in `realism_bands.json`. That is a follow-up, not part of this slice.

### Tuned toward the band, by ticket

| Metric | Before | After | Ticket |
|---|---|---|---|
| Shots | 45.5 | about 29 | PITCH-120, `SHOOT_RANGE` 0.23 to 0.16 |
| Goals | 4.9, then 4.2 | about 3.4 | PITCH-120, PITCH-124 (finishing multiplier 0.78 to 0.60) |
| Final-third time | 40.1% | 28.6% | PITCH-152 |
| Fouls | 5.5 to 7.5 | 11.6 | PITCH-120, 127, 152 (still off) |

## Known-off metrics and the ratchet

PITCH-145 added the ratchet. The rules, which Nutmeg should keep:

1. A metric inside its band must stay inside it.
2. A metric outside its band is listed as known-off with its distance from the band. It may not drift further out.
3. When a known-off metric comes back into band, the test fails until its entry is deleted, so the list can only shrink.
4. A known-off distance is never raised to make a run pass. It is re-measured from a fresh baseline.

Known-off at the end of the PHP engine's life (`REALISM_KNOWN_OFF`, 200-seed baseline):

| Metric | Measured | Band | Outside by |
|---|---|---|---|
| Shots on target | 12.4 | 7.5 to 11 | 1.40 |
| Crosses | 13.9 | 14 to 42 | 0.10 |
| Fouls | 11.6 | 18 to 28 | 6.40 (PITCH-126) |

League: `Goalless %` was 14.8 against 4 to 14 (0.80 outside, 40-seed batch). The resolver had no notion of the scoreline while a match ran, so a nil-nil never opened up. A real fix needs game-state feedback, not a constant.

History worth knowing. At PITCH-145 five metrics were off: shots on target by 2.1, pass completion by 2.7, crosses by 3.3, fouls by 10.5, final-third time by 10.1. Final-third time left the list at PITCH-152. Pass completion left it later (the PITCH-158 commit says cross and pass constants were nudged so the bands hold, and it is absent from the final list). Shots on target and fouls never got fixed.

In `realism_bands.json`, `php_known_off` and `php_last_measured` record that history. The `known_off` field is the Nutmeg engine's own status and starts false for all metrics: Nutmeg has no engine, so nothing is known to be off. It is set only when a baseline is blessed on purpose and a metric is out of band.

## Paired per-seed comparison (PITCH-175)

Why it exists: the ratchet originally ran 40 seeds and compared each known-off distance with a fixed number plus a small slack. Those 40 seeds sat on the good side of where the engine really was. The same engine measured at 40, 200 and 1000 seeds gave band distances of 0.90, 1.40 and 1.40 for shots on target, and 6.10, 6.40 and 6.50 for fouls. Any change that merely re-drew part of a match pulled the 40-seed figure back toward the long-run value, which is outwards, and the check failed although the engine was no worse. PITCH-157 (half-time) hit exactly this: over 1000 paired seeds nothing moved by two standard errors, yet the 40-seed run failed and a one-off waiver was needed.

The fix, as built:

- **Record per-seed counts once.** `RealismBaseline` stores, for seeds 1 to 200, the integer counts behind each known-off metric: shots on target, passes, passes completed, crosses, fouls (`tests/Fixtures/realism-baseline.json`: a column list and one row per seed). Re-recorded with `pitch:realism-baseline --write`.
- **Replay the same seeds** on the changed engine. Seed i plays the same opening minutes before and after a change that only re-draws part of a match, so the per-seed difference carries far less noise than either mean. This is the same idea as `PairedRunner` pairing its two arms.
- **Per-match metrics (a count per match):** per-seed difference is the new count minus the recorded count. The mean of those differences is the shift and its standard error is the sample standard deviation over the square root of n.
- **Percent metrics (a ratio of pooled totals):** linearised around the recorded ratio r. Each seed contributes `numerator - r * denominator`, the differences are averaged, and the result is scaled by `100 / mean(new denominator)`. That makes a percentage also a mean of per-seed differences.
- **Verdict.** Orient the shift outwards (positive if it moved away from the band). Fail when `outwards > 3 * standard error`. If the metric is now inside its band and moved inwards by more than three standard errors, fail until the entry is deleted from the known-off list. Sitting just inside the band is not enough, because a metric a hair outside crosses over on a re-draw alone.
- **Guards.** The comparison refuses a different number of seeds or fewer than two. Unchanged seeds give exactly zero shift and zero error.
- **Cost.** 200 seeds took about 95 s in PHP against about 20 s for 40. Nutmeg's C++ headless runs should be far cheaper, but the seed count and sigma (200 and 3.0) are the PHP values and should be re-evaluated once Nutmeg's per-seed variance is known.

Two requirements follow for the C++ sim. A seed must reproduce the same opening phases when an unrelated subsystem changes, which means separate RNG streams per subsystem (the PHP engine drew from one stream, so a change that shifted consumption early re-drew everything after it). And the baseline file must store per-seed integer counts, not rates, so the shift and its error can be recomputed.

## Other analysis tools worth carrying over

These have no bands, but Nutmeg should plan for them:

- **Legibility experiment (`ExperimentHarness`).** Same seeds twice: both sides even, then one attribute raised on the home side. It reports the home side's goals for and against, shots, conversion, pass completion and possession. The delta isolates one attribute. If the engine is legible, each attribute moves the metrics it should in the expected direction. It exists to catch an attribute that does nothing or points the wrong way. Attribute bump was a delta on one of vision, passing, dribbling, finishing, tackling or pace.
- **Shape (`ShapeHarness`, `ShapeResult`).** Average each player's position over every frame of a batch. Outfield shape metrics are oriented up each side's own attacking axis: line height (mean advance of the back four), length (max minus min advance), width (max minus min lateral position) and compactness (length times width). Used to see that formations differ and block height responds to mentality, not to pass or fail.

## The golden master and why it cannot be reused

What it was (`GoldenMaster`, PITCH-144): a determinism fixture. For seeds 1 to 8 it hashed the full output of three simulation paths with SHA-256: the positional engine (score, every event and every per-tick frame), the open-play engine (raw event stream) and a full fixture between two deliberately unequal sides (24 digests in `tests/Fixtures/golden-master.json`). Floats were formatted with `%.17g` rather than `json_encode`, because the latter depends on the `serialize_precision` ini setting and so could differ between a dev machine and CI. Sides were uneven on purpose so the fingerprint notices a change that only shows when players differ. `pitch:golden-master` checked the fixture and `--write` deliberately re-recorded it.

Why it existed: `RngTest` proves the random stream and `ResumableEngineTest` proves a paused match resumes exactly, but neither notices when a calibration change quietly rewrites every historical result. A moving digest is not a failure in itself, since a calibration ticket should move it, but moving it must be deliberate, reviewable and land in the same commit as the cause.

Why it cannot be reused:

- It fingerprints PHP engine output. The C++ engine has different physics, event and frame structure, and RNG consumption, so not a single digest would match, and matching would prove nothing.
- Two of its three paths (open-play engine, fixture resolver) do not exist in Nutmeg.
- It hashed PHP-specific representations (float formatting, array key order, `toArray()` shapes).
- Bit-exact hashing across compilers, optimisation levels and platforms is a new problem in C++ (floating point contraction, SIMD, libm differences). Whether Nutmeg's goldens are blessed per platform or the sim uses fixed-point or controlled `dmath` is a decision for the engine tickets, and it is a precondition for a hash-based golden master being usable in CI.

So a new golden master is blessed on purpose once the loop and physics stabilise, with the same discipline: a committed digest file, a check that names the diverging seeds, and an explicit command to re-bless it. It complements the bands. The bands say whether the engine is realistic, the golden master says whether it changed at all. Neither replaces the other.

## What cannot be measured until the C++ sim produces events

Every metric above. None can be computed today because there is no simulation. The runner (NUT-33) must emit the fields below. The names are a proposal for NUT-33's result schema.

### Run header

- `schema_version` (int)
- `build` (string, commit hash)
- `seed` (int)
- `home` and `away`: `rating` (int, 1 to 100) or the full attribute set, `formation`, `mentality`, and a `side_set_id` for fixed league sides
- `final_score`: `home` (int), `away` (int)
- `match_seconds` (float, simulated length)
- `sample_hz` (int, the rate of `samples`)

### Events (`events[]`, in time order)

| Field | Type | Notes |
|---|---|---|
| `t` | float | match seconds |
| `type` | enum | `pass`, `cross`, `shot`, `header`, `goal`, `save`, `block`, `foul`, `corner`, `throw_in`, `goal_kick`, `penalty`, `tackle`, `slide_tackle`, `interception`, `clearance`, `yellow_card`, `red_card` (the PHP `EventType` set) |
| `side` | 0 or 1 | acting side, 0 is home |
| `actor` | int | player id, stable within the match |
| `x`, `y` | float | where the event started, normalised pitch coordinates 0 to 1, home attacks toward x = 1 |
| `success` | bool | meaningful for `pass` and `cross` (arrived at a teammate) |
| `shot_outcome` | enum | on `shot` and `header`: `goal`, `saved`, `blocked`, `off_target`, `woodwork` |

Emitting `shot_outcome` explicitly removes the PHP workaround of recovering saves from the next event. A `goal` event should be emitted as well so goals can be counted without joining.

### Samples (`samples[]`, at `sample_hz` while the clock runs)

| Field | Type | Notes |
|---|---|---|
| `t` | float | match seconds |
| `ball_x`, `ball_y` | float | normalised, same orientation as events |
| `possession` | 0, 1 | the side that last controlled the ball. Never null, loose balls keep the last controller, which matches the PHP frames |
| `players` | optional list of `[x, y]` | 22 entries, home keeper first then outfield, then away, only needed for shape and only when asked for |

PHP counted every frame it emitted, including dead balls and set pieces. The runner must state `sample_hz` and the NUT-33 schema should say whether dead-ball time is sampled, because it changes the possession and final-third denominators.

### Metric to field map

| Metric | Computed from |
|---|---|
| `goals_per_match` | count of `goal` events, or `final_score` sum |
| `shots_per_match` | count of `shot` plus `header` events |
| `shots_on_target_per_match` | `shot`/`header` with `shot_outcome` in `goal`, `saved` |
| `shot_conversion_pct` | `goal` over `shot` plus `header`, pooled |
| `save_pct` | `shot`/`header` with `shot_outcome` `saved`, over those with `goal` or `saved`, pooled. Derived from the shots on target and goals bands: about 70 percent. Checked by `gameplay/tests/test_keeper.cpp` |
| `pass_completion_pct` | `pass` events with `success`, over all `pass` events, pooled |
| `crosses_per_match` | count of `cross` events |
| `fouls_per_match` | count of `foul` events |
| `corners_per_match` | count of `corner` events |
| `home_possession_pct` | samples with `possession` = 0 over all samples, pooled |
| `final_third_time_pct` | samples where `ball_x` oriented by `possession` exceeds 0.66, over all samples, pooled |
| `league_*` | `final_score` per fixture, plus `side_set_id` to confirm the fixed division |
| paired check | per-seed integer counts: shots on target, passes, completed passes, crosses, fouls, and any other known-off metric's numerator and denominator |
| shape diagnostics | `players` in `samples` |

The baseline file for the paired check should be written by the runner from the same fields: one row per seed, integer columns, so the shift and standard error can be recomputed without re-simulating. For metrics that are percentages, store both numerator and denominator.

### Suggested extras

Not needed by any current band, but cheap to emit now and expensive to retrofit: shot origin distance in metres (PHP kept `shotAdvanceSum` and never used it), the first-contact and delivery times of set pieces, and the possession-change count (PITCH-152 diagnosed the final-third problem by noticing possession changed hands only every four seconds).

## Open questions for the engine tickets

- Real-time play means Nutmeg matches have a human on one side. The harness is AI against AI at even ratings. Decide what an "AI vs AI" batch means for controller input (a scripted or default AI controller for both sides) before bands are checked in CI.
- Validate the unsourced bands against public data before they become gates.
- Choose seed count and sigma after the first real variance measurements.
- Decide whether `home_possession_pct` should stay a banded metric when sides are symmetric, or become a symmetry test with a tighter band.
