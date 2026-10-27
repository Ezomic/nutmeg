#pragma once

#include "nutmeg/gameplay/actions.h"
#include "nutmeg/gameplay/events.h"
#include "nutmeg/gameplay/player.h"
#include "nutmeg/gameplay/referee.h"
#include "nutmeg/physics/vec.h"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace nutmeg::gameplay {

// Tuning for restarts. Distances are metres, speeds m/s, times ticks.
namespace setpiece {

// The ball stays dead this long after it leaves play before the restart is set up.
constexpr int kDeadBallTicks = 90;

constexpr float kInset = 0.05f;
constexpr float kTakerDistance = 0.6f;
constexpr float kClearRadius = 9.15f;
constexpr float kThrowClearRadius = 2.0f;

constexpr float kGoalKickDepth = 2.75f;
constexpr float kGoalKickOffset = 4.0f;
constexpr float kPenaltyClearDepth = 20.3f;
constexpr float kKeeperDepth = 0.3f;

constexpr float kThrowSpeedMax = 15.0f;
constexpr float kThrowLift = 3.0f;
constexpr float kThrowReleaseHeight = 1.9f;


// The restart that follows a ball leaving play, or nullopt for any other event.
// A goal is restarted by the side that conceded it; the ball leaving over a
// touchline or goal line is a throw-in, goal kick or corner. `ball` is where
// the ball was when it left.
std::optional<Restart> restartAfter(const MatchEvent& event, physics::Vec2 ball);

// Picks the taker, stands them next to the ball facing the way they will kick,
// and puts the penalty keeper on the goal line. Fills in `restart.taker`.
void arrange(Player* players, std::size_t count, Restart& restart);

// Moves every player the Laws keep back from the ball out of the way: the
// opposition from the ball, both teams to their own half at a kickoff, all but
// the taker and keeper out of the area and arc at a penalty.
void keepClear(Player* players, std::size_t count, const Restart& restart);

// What a taker's request becomes: a throw-in is always a throw, a corner is a cross.
ActionRequest shapeRequest(const Restart& restart, ActionRequest request);

// A throw-in leaves the hands: capped speed, lifted, released from head height.
void shapeKick(const Restart& restart, Kick& kick, physics::Vec3& ballPosition);

// A goal cannot be scored straight from these, and nobody can be offside from them.
bool scoresIndirectly(RestartKind kind);
bool exemptFromOffside(RestartKind kind);

}

}
