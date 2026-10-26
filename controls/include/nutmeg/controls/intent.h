#pragma once

#include "nutmeg/controls/charge.h"
#include "nutmeg/controls/control_state.h"
#include "nutmeg/controls/stick.h"
#include "nutmeg/controls/vec.h"
#include "nutmeg/core/input.h"

namespace nutmeg::controls {

// A kick button. charging is true while it is held, with the power it would
// have if released now; fire is true on the one tick it is released.
struct KickIntent {
    bool charging = false;
    bool fire = false;
    float power = 0.0f;

    bool operator==(const KickIntent&) const = default;
};

struct Intent {
    Vec2 move; // pitch coordinates, length at most 1
    bool sprint = false;
    KickIntent pass;
    KickIntent through;
    KickIntent shoot;
    bool tackle = false;       // true on the tick the button goes down
    bool switchPlayer = false; // true on the tick the button goes down
    bool pause = false;        // true on the tick the button goes down

    bool operator==(const Intent&) const = default;
};

struct IntentConfig {
    StickConfig stick;
    ChargeConfig charge;
    // +1 when the controlled team attacks towards +x, -1 towards -x. InputFrame
    // moveY is "towards the opponent's goal", so this fixes the pitch frame.
    float attackDirection = 1.0f;
};

// `state` must already have been updated with `frame` this tick. Pitch x runs
// along the pitch and y across it; facing +x, the right touchline is -y.
Intent desiredIntent(const InputFrame& frame, const ControlState& state, const IntentConfig& config = {});

}
