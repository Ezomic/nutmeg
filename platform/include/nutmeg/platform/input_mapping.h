#pragma once

#include "nutmeg/core/input.h"

namespace nutmeg {

struct KeyboardState {
    bool left = false;
    bool right = false;
    bool up = false;
    bool down = false;
    bool pass = false;
    bool through = false;
    bool shoot = false;
    bool sprint = false;
    bool tackle = false;
    bool switchPlayer = false;
    bool pause = false;
};

// Stick axes use SDL's convention: y grows downwards.
struct GamepadState {
    bool connected = false;
    float stickX = 0.0f;
    float stickY = 0.0f;
    bool dpadLeft = false;
    bool dpadRight = false;
    bool dpadUp = false;
    bool dpadDown = false;
    bool south = false;
    bool north = false;
    bool east = false;
    bool west = false;
    bool leftShoulder = false;
    bool rightShoulder = false;
    bool start = false;
};

constexpr float kStickDeadzone = 0.2f;

InputFrame mapKeyboard(const KeyboardState& keys);
InputFrame mapGamepad(const GamepadState& pad);

// Buttons are ORed together; movement comes from whichever source pushes harder.
InputFrame mergeInput(const InputFrame& a, const InputFrame& b);

}
