#pragma once

#include "nutmeg/gameplay/actions.h"
#include "nutmeg/gameplay/world.h"
#include "nutmeg/physics/pitch.h"

namespace nutmeg::ai {

// Tuning for AI set pieces. Distances are metres, times ticks.
namespace setpiece_tuning {

// The taker waits this long after the restart is set up, so the team can get into position.
constexpr int kTakeDelayTicks = 40;
// A corner or free kick waits longer, so attackers can run into the box.
constexpr int kSetupDelayTicks = 180;

constexpr float kMateMinDistance = 4.0f;
constexpr float kThrowMaxDistance = 22.0f;
constexpr float kGoalKickMinDistance = 15.0f;
constexpr float kOpenWeight = 1.0f;
constexpr float kProgressWeight = 0.3f;
constexpr float kDistanceWeight = 0.1f;

constexpr float kShootRange = 30.0f;
constexpr float kChipRange = 28.0f;
constexpr float kCrossMinX = 10.0f;
constexpr float kShotPower = 0.95f;
constexpr float kCornerAimFraction = 0.8f;

// How many attackers join the box for a corner or a free kick near goal.
constexpr int kBoxRunners = 4;
constexpr float kBoxRange = 35.0f;

}

// What `restart`'s taker does once the delay has passed: a kick request for
// the taker, or an empty request while it waits. A pure function of the world.
gameplay::ActionRequest planRestart(const gameplay::World& world, const gameplay::Restart& restart);

// Sends the taking side's attackers into the box for a corner or free kick near goal.
void positionForRestart(const gameplay::World& world, const gameplay::Restart& restart, gameplay::PlayerInputs& inputs);

}
