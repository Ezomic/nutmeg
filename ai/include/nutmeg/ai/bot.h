#pragma once

#include "nutmeg/ai/decision.h"
#include "nutmeg/ai/team_shape.h"
#include "nutmeg/core/input.h"
#include "nutmeg/gameplay/world.h"

namespace nutmeg::ai {

// Stick and sprint that carry `player` towards `target`, in the frame World
// expects. Eases off inside kArriveRadius so the player settles on the spot.
InputFrame moveToward(const gameplay::Player& player, const PlayerTarget& target);

// World x of the line a team attacking the far goal must stay behind: the
// second-last defender, but never behind the ball or the halfway line.
float offsideLine(const gameplay::World& world, physics::Side attacking);

TeamInput inputFor(const gameplay::World& world, physics::Side side, Formation formation, Mentality mentality, BallControl control);

// Plans `side` and writes its ten outfielders' inputs. The keeper is left alone.
TeamIntent drive(const gameplay::World& world, physics::Side side, Formation formation, Mentality mentality, BallControl control, gameplay::PlayerInputs& inputs);


// As above, then lets `side`'s ball carrier decide: it kicks through `actions`,
// or steers with `inputs` when the decision is to dribble or hold.
TeamIntent drive(const gameplay::World& world, physics::Side side, Formation formation, Mentality mentality, BallControl control,
    gameplay::PlayerInputs& inputs, gameplay::PlayerActions& actions, Decider& decider);

}
