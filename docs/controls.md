# Controls

Gamepad first, keyboard as the fallback. The platform layer turns a device into an `InputFrame` (see `platform/` `input_mapping.h`); everything after that, from edges and charge to which player you control, lives in `controls/` and is deterministic, so a recorded stream of `InputFrame`s replays identically.

## Default bindings

| Action | Gamepad | Keyboard |
| --- | --- | --- |
| Move | Left stick, d-pad | WASD, arrow keys |
| Pass | South (A / Cross) | J, Space |
| Through ball | North (Y / Triangle) | K |
| Shoot | East (B / Circle) | L |
| Tackle | West (X / Square) | E |
| Sprint | Right shoulder | Shift |
| Switch player | Left shoulder | Q |
| Pause | Start | P |

Pass, through ball and shoot are charged: tap for a soft touch, hold to power up, release to kick. Full power takes 45 ticks (0.75 s) by default; a tap gives 20 % power, with a smoothstep between.

## Moving

Stick input gets a radial deadzone (0.2) with the remainder rescaled to 0..1, so movement eases in instead of jumping. The keyboard and d-pad can be snapped to eight directions with `StickConfig::snapToEightWay`. The result is rotated into pitch coordinates by the team's attack direction: `x` along the pitch, `y` across it, and facing `+x` the right touchline is `-y`.

## Who you control

`ControllerSwitcher` picks the controlled player from the team's positions and the ball:

- Auto-switch: control moves to the teammate nearest the ball, but only once that player is at least 2 m closer than the current one, so a ball near the midpoint does not flip control every tick.
- Manual switch: the Switch button steps through the team in nearest-to-ball order, wrapping around, and pauses auto-switch for 60 ticks so it does not snap straight back.
- Lock: `SwitchInput::lockedTo` pins control to one player, for example the keeper on a goal kick.
- Prediction: `SwitcherConfig::predictSeconds` aims at where the ball will be instead of where it is (off by default).

Rebinding is not implemented yet; the defaults above are fixed in the platform layer.
