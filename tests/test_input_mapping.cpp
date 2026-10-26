#include "nutmeg/platform/input_mapping.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

using namespace nutmeg;
using Catch::Approx;

TEST_CASE("no keys held gives an empty frame")
{
    REQUIRE(mapKeyboard({}) == InputFrame{});
}

TEST_CASE("keyboard directions map to the move axes")
{
    KeyboardState keys;
    keys.right = true;
    keys.up = true;
    const InputFrame frame = mapKeyboard(keys);
    REQUIRE(frame.moveX == Approx(std::sqrt(0.5f)));
    REQUIRE(frame.moveY == Approx(std::sqrt(0.5f)));

    KeyboardState left;
    left.left = true;
    REQUIRE(mapKeyboard(left).moveX == -1.0f);
    KeyboardState down;
    down.down = true;
    REQUIRE(mapKeyboard(down).moveY == -1.0f);
}

TEST_CASE("opposing keys cancel")
{
    KeyboardState keys;
    keys.left = true;
    keys.right = true;
    REQUIRE(mapKeyboard(keys).moveX == 0.0f);
}

TEST_CASE("keyboard actions map to buttons")
{
    KeyboardState keys;
    keys.pass = true;
    keys.shoot = true;
    keys.sprint = true;
    REQUIRE(mapKeyboard(keys).buttons == (InputFrame::Pass | InputFrame::Shoot | InputFrame::Sprint));

    KeyboardState rest;
    rest.through = true;
    rest.tackle = true;
    rest.switchPlayer = true;
    rest.pause = true;
    REQUIRE(mapKeyboard(rest).buttons == (InputFrame::Through | InputFrame::Tackle | InputFrame::Switch | InputFrame::Pause));
}

TEST_CASE("a disconnected pad gives an empty frame")
{
    GamepadState pad;
    pad.south = true;
    pad.stickX = 1.0f;
    REQUIRE(mapGamepad(pad) == InputFrame{});
}

TEST_CASE("the stick has a deadzone and flips the y axis")
{
    GamepadState pad;
    pad.connected = true;
    pad.stickX = 0.1f;
    pad.stickY = 0.1f;
    REQUIRE(mapGamepad(pad) == InputFrame{});

    pad.stickX = 0.0f;
    pad.stickY = -1.0f;
    const InputFrame up = mapGamepad(pad);
    REQUIRE(up.moveX == Approx(0.0f));
    REQUIRE(up.moveY == Approx(1.0f));

    pad.stickX = 0.6f;
    pad.stickY = 0.0f;
    const InputFrame half = mapGamepad(pad);
    REQUIRE(half.moveX == Approx((0.6f - kStickDeadzone) / (1.0f - kStickDeadzone)));
}

TEST_CASE("the d-pad overrides the stick and gamepad buttons map to actions")
{
    GamepadState pad;
    pad.connected = true;
    pad.dpadLeft = true;
    pad.stickX = 1.0f;
    pad.south = true;
    pad.north = true;
    pad.east = true;
    pad.west = true;
    pad.rightShoulder = true;
    pad.leftShoulder = true;
    pad.start = true;
    const InputFrame frame = mapGamepad(pad);
    REQUIRE(frame.moveX == -1.0f);
    REQUIRE(frame.buttons == (InputFrame::Pass | InputFrame::Through | InputFrame::Shoot | InputFrame::Tackle | InputFrame::Sprint | InputFrame::Switch | InputFrame::Pause));
}

TEST_CASE("merging ORs the buttons and keeps the stronger movement")
{
    const InputFrame a{InputFrame::Pass, 0.2f, 0.0f};
    const InputFrame b{InputFrame::Shoot, -0.8f, 0.0f};
    const InputFrame merged = mergeInput(a, b);
    REQUIRE(merged.buttons == (InputFrame::Pass | InputFrame::Shoot));
    REQUIRE(merged.moveX == -0.8f);
}
