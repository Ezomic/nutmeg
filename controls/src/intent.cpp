#include "nutmeg/controls/intent.h"

namespace nutmeg::controls {

namespace {

KickIntent kick(const ControlState& state, InputFrame::Button button, const ChargeConfig& charge)
{
    KickIntent k;
    if (state.held(button)) {
        k.charging = true;
        k.power = chargePower(state.heldTicks(button), charge);
    } else if (state.released(button)) {
        k.fire = true;
        k.power = chargePower(state.releasedAfterTicks(button), charge);
    }
    return k;
}

}

Intent desiredIntent(const InputFrame& frame, const ControlState& state, const IntentConfig& config)
{
    const Vec2 stick = processStick({frame.moveX, frame.moveY}, config.stick);
    Intent intent;
    intent.move = {stick.y * config.attackDirection, -stick.x * config.attackDirection};
    intent.sprint = state.held(InputFrame::Sprint);
    intent.pass = kick(state, InputFrame::Pass, config.charge);
    intent.through = kick(state, InputFrame::Through, config.charge);
    intent.shoot = kick(state, InputFrame::Shoot, config.charge);
    intent.tackle = state.pressed(InputFrame::Tackle);
    intent.switchPlayer = state.pressed(InputFrame::Switch);
    intent.pause = state.pressed(InputFrame::Pause);
    return intent;
}

}
