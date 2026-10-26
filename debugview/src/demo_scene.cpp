#include "nutmeg/debugview/demo_scene.h"

#include <cmath>
#include <format>
#include <numbers>

namespace nutmeg::debugview {

namespace {

constexpr float kPi = std::numbers::pi_v<float>;

// 4-4-2 for the left-to-right side, in the home half; away mirrors it.
constexpr Vec2 kFormation[RenderState::kTeamSize] = {
    {-48.0f, 0.0f},
    {-34.0f, -22.0f}, {-36.0f, -7.0f}, {-36.0f, 7.0f}, {-34.0f, 22.0f},
    {-18.0f, -24.0f}, {-20.0f, -8.0f}, {-20.0f, 8.0f}, {-18.0f, 24.0f},
    {-6.0f, -9.0f}, {-6.0f, 9.0f},
};

Vec2 jogPosition(Vec2 home, int index, double seconds, float side)
{
    const float phase = static_cast<float>(seconds) * 0.6f + static_cast<float>(index) * 0.9f;
    return {side * home.x + 4.0f * std::cos(phase), home.y + 3.0f * std::sin(phase * 1.3f)};
}

Vec2 velocity(Vec2 home, int index, double seconds, float side)
{
    const Vec2 a = jogPosition(home, index, seconds, side);
    const Vec2 b = jogPosition(home, index, seconds + 0.05, side);
    return {b.x - a.x, b.y - a.y};
}

PlayerView makePlayer(Vec2 home, int index, double seconds, float side)
{
    PlayerView player;
    player.position = jogPosition(home, index, seconds, side);
    const Vec2 v = velocity(home, index, seconds, side);
    player.facing = std::atan2(v.y, v.x);
    player.number = index + 1;
    return player;
}

}

RenderState demoScene(double seconds)
{
    RenderState state;
    for (int i = 0; i < RenderState::kTeamSize; ++i) {
        state.home[i] = makePlayer(kFormation[i], i, seconds, 1.0f);
        state.away[i] = makePlayer({-kFormation[i].x, kFormation[i].y}, i, seconds, 1.0f);
        state.away[i].number = i + 1;
    }

    constexpr double kLegSeconds = 1.2;
    constexpr int kChain[] = {6, 7, 9, 8, 10, 7};
    constexpr int kChainLength = sizeof(kChain) / sizeof(kChain[0]);
    const double legs = seconds / kLegSeconds;
    const int leg = static_cast<int>(std::floor(legs));
    const float t = static_cast<float>(legs - std::floor(legs));
    const int from = kChain[((leg % kChainLength) + kChainLength) % kChainLength];
    const int to = kChain[(((leg + 1) % kChainLength) + kChainLength) % kChainLength];

    const Vec2 a = state.home[from].position;
    const Vec2 b = state.home[to].position;
    state.ball.position = {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
    state.ball.height = 1.5f * std::sin(kPi * t);
    state.home[from].controlled = t < 0.5f;
    state.home[to].controlled = t >= 0.5f;

    state.overlayText = std::format("t={:.1f}s  ball {:.1f},{:.1f}", seconds, state.ball.position.x, state.ball.position.y);
    return state;
}

}
