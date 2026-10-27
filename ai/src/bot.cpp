#include "nutmeg/ai/bot.h"

#include <algorithm>

namespace nutmeg::ai {

namespace {

using gameplay::kPlayersPerSide;
using physics::Side;
using physics::Vec2;

std::size_t firstIndex(Side side) { return side == Side::Home ? 0 : kPlayersPerSide; }

}

InputFrame moveToward(const gameplay::Player& player, const PlayerTarget& target)
{
    InputFrame frame;
    const Vec2 d = target.point - player.position;
    const float dist = physics::length(d);
    if (dist < 1e-4f) {
        return frame;
    }
    const float stick = std::min(dist / tuning::kArriveRadius, 1.0f);
    const Vec2 dir = d * (stick / dist);
    if (player.side == Side::Home) {
        frame.moveY = dir.x;
        frame.moveX = -dir.y;
    } else {
        frame.moveY = -dir.x;
        frame.moveX = dir.y;
    }
    if (target.urgency >= tuning::kSprintUrgency && dist > tuning::kArriveRadius) {
        frame.buttons |= InputFrame::Sprint;
    }
    return frame;
}

float offsideLine(const gameplay::World& world, Side attacking)
{
    const float sign = attacking == Side::Home ? 1.0f : -1.0f;
    const std::size_t base = firstIndex(physics::opposite(attacking));
    float deepest = -physics::pitch::kHalfLength;
    float second = -physics::pitch::kHalfLength;
    for (std::size_t i = 0; i < kPlayersPerSide; ++i) {
        const float x = world.players()[base + i].position.x * sign;
        if (x > deepest) {
            second = deepest;
            deepest = x;
        } else if (x > second) {
            second = x;
        }
    }
    const float ball = world.ball().position.x * sign;
    const float line = std::max(std::max(second, ball), 0.0f);
    return line * sign;
}

TeamInput inputFor(const gameplay::World& world, Side side, Formation formation, Mentality mentality, BallControl control)
{
    TeamInput in;
    in.side = side;
    in.formation = formation;
    in.mentality = mentality;
    in.control = control;
    in.ball = physics::xy(world.ball().position);
    const std::size_t own = firstIndex(side);
    const std::size_t other = firstIndex(physics::opposite(side));
    for (std::size_t i = 0; i < kOutfielders; ++i) {
        in.positions[i] = world.players()[own + 1 + i].position;
        in.opponents[i] = world.players()[other + 1 + i].position;
    }
    in.offsideLineX = offsideLine(world, side);
    return in;
}

TeamIntent drive(const gameplay::World& world, Side side, Formation formation, Mentality mentality, BallControl control, gameplay::PlayerInputs& inputs)
{
    const TeamIntent intent = planTeam(inputFor(world, side, formation, mentality, control));
    const std::size_t own = firstIndex(side);
    for (std::size_t i = 0; i < kOutfielders; ++i) {
        inputs[own + 1 + i] = moveToward(world.players()[own + 1 + i], intent.targets[i]);
    }
    return intent;
}

TeamIntent drive(const gameplay::World& world, Side side, Formation formation, Mentality mentality, BallControl control,
    gameplay::PlayerInputs& inputs, gameplay::PlayerActions& actions, Decider& decider)
{
    const TeamIntent intent = drive(world, side, formation, mentality, control, inputs);
    const std::uint32_t chaser = looseBallChaser(world, side);
    if (chaser != gameplay::kNoPlayer) {
        inputs[chaser] = moveToward(world.players()[chaser], {physics::xy(world.ball().position), tuning::kPressUrgency});
    }
    const CarrierPlan plan = decider.plan(world, side);
    if (plan.player == gameplay::kNoPlayer) {
        return intent;
    }
    actions[plan.player] = plan.request;
    if (plan.steer) {
        inputs[plan.player] = moveToward(world.players()[plan.player], {plan.destination, tuning::kSprintUrgency});
    }
    return intent;
}

}
