#include "nutmeg/controls/charge.h"
#include "nutmeg/controls/control_state.h"
#include "nutmeg/controls/controller_switcher.h"
#include "nutmeg/controls/intent.h"
#include "nutmeg/controls/stick.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

using namespace nutmeg;
using namespace nutmeg::controls;
using Catch::Approx;

namespace {

InputFrame buttons(std::uint32_t mask)
{
    InputFrame f;
    f.buttons = mask;
    return f;
}

}

TEST_CASE("ControlState reports press, hold and release edges")
{
    ControlState state;
    state.update(buttons(InputFrame::Pass));
    CHECK(state.pressed(InputFrame::Pass));
    CHECK(state.held(InputFrame::Pass));
    CHECK_FALSE(state.released(InputFrame::Pass));
    CHECK(state.heldTicks(InputFrame::Pass) == 1);

    state.update(buttons(InputFrame::Pass));
    CHECK_FALSE(state.pressed(InputFrame::Pass));
    CHECK(state.held(InputFrame::Pass));
    CHECK(state.heldTicks(InputFrame::Pass) == 2);

    state.update(buttons(0));
    CHECK(state.released(InputFrame::Pass));
    CHECK_FALSE(state.held(InputFrame::Pass));
    CHECK(state.releasedAfterTicks(InputFrame::Pass) == 2);
    CHECK(state.heldTicks(InputFrame::Pass) == 0);

    state.update(buttons(0));
    CHECK_FALSE(state.released(InputFrame::Pass));
    CHECK(state.releasedAfterTicks(InputFrame::Pass) == 0);
}

TEST_CASE("ControlState tracks buttons independently and restarts the count")
{
    ControlState state;
    state.update(buttons(InputFrame::Shoot));
    state.update(buttons(InputFrame::Shoot | InputFrame::Pass));
    CHECK(state.heldTicks(InputFrame::Shoot) == 2);
    CHECK(state.heldTicks(InputFrame::Pass) == 1);
    CHECK(state.pressed(InputFrame::Pass));
    CHECK_FALSE(state.pressed(InputFrame::Shoot));

    state.update(buttons(0));
    state.update(buttons(InputFrame::Shoot));
    CHECK(state.heldTicks(InputFrame::Shoot) == 1);
}

TEST_CASE("chargePower rises monotonically from the minimum to full")
{
    const ChargeConfig config;
    CHECK(chargePower(0, config) == Approx(config.minPower));
    CHECK(chargePower(config.fullChargeTicks, config) == Approx(1.0f));
    CHECK(chargePower(config.fullChargeTicks * 10, config) == Approx(1.0f));
    CHECK(chargePower(config.fullChargeTicks / 2, config) == Approx((config.minPower + 1.0f) * 0.5f).margin(0.02));

    float previous = chargePower(0, config);
    for (int t = 1; t <= config.fullChargeTicks; ++t) {
        const float p = chargePower(t, config);
        CHECK(p >= previous);
        previous = p;
    }
}

TEST_CASE("processStick applies a radial deadzone and rescales")
{
    StickConfig config;
    config.deadzone = 0.2f;
    CHECK(processStick({0.1f, 0.1f}, config) == Vec2{});
    CHECK(processStick({0.2f, 0.0f}, config) == Vec2{});

    const Vec2 half = processStick({0.6f, 0.0f}, config);
    CHECK(half.x == Approx(0.5f));
    CHECK(half.y == Approx(0.0f));

    const Vec2 over = processStick({3.0f, 4.0f}, config);
    CHECK(std::sqrt(over.x * over.x + over.y * over.y) == Approx(1.0f));
    CHECK(over.x == Approx(0.6f));
    CHECK(over.y == Approx(0.8f));
}

TEST_CASE("processStick deadzone is radial, not per axis")
{
    const Vec2 out = processStick({0.15f, 0.9f});
    CHECK(out.x > 0.0f);
    CHECK(out.y > 0.0f);
}

TEST_CASE("processStick snaps to eight directions")
{
    StickConfig config;
    config.snapToEightWay = true;

    const Vec2 nearlyRight = processStick({1.0f, 0.2f}, config);
    CHECK(nearlyRight.x == Approx(1.0f));
    CHECK(nearlyRight.y == Approx(0.0f));

    const Vec2 diagonal = processStick({0.8f, 0.7f}, config);
    CHECK(diagonal.x == Approx(0.70710678f));
    CHECK(diagonal.y == Approx(0.70710678f));

    const Vec2 downLeft = processStick({-0.5f, -0.6f}, config);
    CHECK(downLeft.x < 0.0f);
    CHECK(downLeft.y < 0.0f);
}

TEST_CASE("desiredIntent maps stick to pitch coordinates for either attacking direction")
{
    InputFrame frame;
    frame.moveY = 1.0f;
    ControlState state;
    state.update(frame);

    IntentConfig config;
    config.attackDirection = 1.0f;
    CHECK(desiredIntent(frame, state, config).move.x == Approx(1.0f));
    config.attackDirection = -1.0f;
    CHECK(desiredIntent(frame, state, config).move.x == Approx(-1.0f));

    InputFrame right;
    right.moveX = 1.0f;
    state.update(right);
    config.attackDirection = 1.0f;
    CHECK(desiredIntent(right, state, config).move.y == Approx(-1.0f));
    config.attackDirection = -1.0f;
    CHECK(desiredIntent(right, state, config).move.y == Approx(1.0f));
}

TEST_CASE("desiredIntent charges a kick while held and fires on release with the held power")
{
    ControlState state;
    const IntentConfig config;

    state.update(buttons(InputFrame::Shoot));
    Intent first = desiredIntent(buttons(InputFrame::Shoot), state, config);
    CHECK(first.shoot.charging);
    CHECK_FALSE(first.shoot.fire);

    for (int i = 0; i < config.charge.fullChargeTicks; ++i) {
        state.update(buttons(InputFrame::Shoot));
    }
    const Intent charged = desiredIntent(buttons(InputFrame::Shoot), state, config);
    CHECK(charged.shoot.power == Approx(1.0f));

    state.update(buttons(0));
    const Intent released = desiredIntent(buttons(0), state, config);
    CHECK(released.shoot.fire);
    CHECK_FALSE(released.shoot.charging);
    CHECK(released.shoot.power == Approx(1.0f));

    state.update(buttons(0));
    CHECK_FALSE(desiredIntent(buttons(0), state, config).shoot.fire);
}

TEST_CASE("desiredIntent: a tap passes softly")
{
    ControlState state;
    state.update(buttons(InputFrame::Pass));
    state.update(buttons(0));
    const Intent intent = desiredIntent(buttons(0), state);
    CHECK(intent.pass.fire);
    CHECK(intent.pass.power < 0.3f);
    CHECK_FALSE(intent.through.fire);
    CHECK_FALSE(intent.shoot.fire);
}

TEST_CASE("desiredIntent: tackle and switch fire on the press edge only, sprint while held")
{
    ControlState state;
    const std::uint32_t all = InputFrame::Tackle | InputFrame::Switch | InputFrame::Sprint;
    state.update(buttons(all));
    Intent a = desiredIntent(buttons(all), state);
    CHECK(a.tackle);
    CHECK(a.switchPlayer);
    CHECK(a.sprint);

    state.update(buttons(all));
    Intent b = desiredIntent(buttons(all), state);
    CHECK_FALSE(b.tackle);
    CHECK_FALSE(b.switchPlayer);
    CHECK(b.sprint);
}

TEST_CASE("ControllerSwitcher picks the player nearest the ball at first")
{
    const std::vector<Vec2> players = {{-20, 0}, {5, 0}, {30, 0}};
    ControllerSwitcher switcher;
    CHECK(switcher.update({.players = players, .ball = {4, 1}}) == 1);
}

TEST_CASE("ControllerSwitcher auto-switches when another player is clearly closer")
{
    std::vector<Vec2> players = {{0, 0}, {20, 0}};
    ControllerSwitcher switcher;
    CHECK(switcher.update({.players = players, .ball = {1, 0}}) == 0);
    CHECK(switcher.update({.players = players, .ball = {19, 0}}) == 1);
}

TEST_CASE("ControllerSwitcher does not flicker with the ball oscillating around the midpoint")
{
    const std::vector<Vec2> players = {{0, 0}, {10, 0}};
    ControllerSwitcher switcher;
    switcher.update({.players = players, .ball = {2, 0}});

    int changes = 0;
    std::size_t previous = switcher.current();
    for (int tick = 0; tick < 600; ++tick) {
        const float x = 5.0f + (tick % 2 == 0 ? 0.8f : -0.8f);
        const std::size_t now = switcher.update({.players = players, .ball = {x, 0}});
        if (now != previous) {
            ++changes;
        }
        previous = now;
    }
    CHECK(changes == 0);
}

TEST_CASE("ControllerSwitcher manual switch cycles by nearest-to-ball order and pauses auto-switch")
{
    const std::vector<Vec2> players = {{0, 0}, {4, 0}, {12, 0}};
    const Vec2 ball{1, 0};
    ControllerSwitcher switcher;
    CHECK(switcher.update({.players = players, .ball = ball}) == 0);
    CHECK(switcher.update({.players = players, .ball = ball, .manualSwitch = true}) == 1);
    CHECK(switcher.update({.players = players, .ball = ball}) == 1);
    CHECK(switcher.update({.players = players, .ball = ball, .manualSwitch = true}) == 2);
    CHECK(switcher.update({.players = players, .ball = ball, .manualSwitch = true}) == 0);
}

TEST_CASE("ControllerSwitcher resumes auto-switching after the manual hold")
{
    const std::vector<Vec2> players = {{0, 0}, {30, 0}};
    const Vec2 ball{0, 0};
    SwitcherConfig config;
    config.manualHoldTicks = 5;
    ControllerSwitcher switcher(config);
    switcher.update({.players = players, .ball = ball});
    CHECK(switcher.update({.players = players, .ball = ball, .manualSwitch = true}) == 1);
    for (int i = 0; i < 4; ++i) {
        CHECK(switcher.update({.players = players, .ball = ball}) == 1);
    }
    CHECK(switcher.update({.players = players, .ball = ball}) == 0);
}

TEST_CASE("ControllerSwitcher lock pins control to one player")
{
    const std::vector<Vec2> players = {{-50, 0}, {0, 0}};
    ControllerSwitcher switcher;
    CHECK(switcher.update({.players = players, .ball = {0, 0}, .manualSwitch = true, .lockedTo = 0}) == 0);
    CHECK(switcher.update({.players = players, .ball = {0, 0}, .lockedTo = 0}) == 0);
    CHECK(switcher.update({.players = players, .ball = {0, 0}}) == 1);
}

TEST_CASE("ControllerSwitcher can aim at the predicted ball path")
{
    const std::vector<Vec2> players = {{0, 0}, {20, 0}};
    SwitcherConfig config;
    config.predictSeconds = 1.0f;
    ControllerSwitcher switcher(config);
    CHECK(switcher.update({.players = players, .ball = {2, 0}, .ballVelocity = {16, 0}}) == 1);
}

TEST_CASE("controls are deterministic")
{
    const std::vector<Vec2> players = {{-10, 3}, {2, -4}, {9, 8}, {15, -1}};
    auto run = [&] {
        ControlState state;
        ControllerSwitcher switcher;
        std::vector<float> trace;
        for (int tick = 0; tick < 300; ++tick) {
            InputFrame frame;
            frame.buttons = (tick % 7 < 3 ? InputFrame::Shoot : 0u) | (tick % 11 == 0 ? InputFrame::Switch : 0u);
            frame.moveX = std::sin(tick * 0.1f);
            frame.moveY = std::cos(tick * 0.13f);
            state.update(frame);
            const Intent intent = desiredIntent(frame, state);
            const Vec2 ball{static_cast<float>(tick % 40) - 10.0f, 0.0f};
            const std::size_t who = switcher.update({.players = players, .ball = ball, .manualSwitch = intent.switchPlayer});
            trace.push_back(intent.move.x);
            trace.push_back(intent.move.y);
            trace.push_back(intent.shoot.power);
            trace.push_back(static_cast<float>(who));
        }
        return trace;
    };
    CHECK(run() == run());
}
