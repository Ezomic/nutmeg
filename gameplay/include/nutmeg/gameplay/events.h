#pragma once

#include "nutmeg/gameplay/actions.h"
#include "nutmeg/physics/ball.h"
#include "nutmeg/physics/pitch.h"

#include <cstdint>

namespace nutmeg::gameplay {

enum class EventKind : std::uint8_t {
    Touch,        // value: first touch quality, 0 for a heavy touch or a deflection
    Pass,         // action says which; value: launch speed in m/s
    Shot,         // value: launch speed in m/s
    Tackle,       // other: the ball carrier; value: 1 if the tackler won the ball, else 0
    FoulCandidate, // player: the tackler, other: the victim; the referee decides later
    Goal,         // side: the team that scored; player: last to touch the ball
    OutOfPlay,    // side and player: the last team and player to touch the ball
    Save,         // player: the keeper, other: the shooter; value: 1 if held, 0 if parried
    Claim,        // player: the keeper; value: 1 if held, 0 if fumbled or punched away
};

struct MatchEvent {
    EventKind kind = EventKind::Touch;
    std::uint64_t tick = 0;
    std::uint32_t player = kNoPlayer;
    std::uint32_t other = kNoPlayer;
    physics::Side side = physics::Side::Home;
    ActionKind action = ActionKind::None;
    physics::Line line = physics::Line::None;
    float value = 0.0f;

    bool operator==(const MatchEvent&) const = default;
};

}
