#include "nutmeg/platform/input_mapping.h"

#include <algorithm>
#include <cmath>

namespace nutmeg {

namespace {

void limitToUnitCircle(float& x, float& y)
{
    const float length = std::sqrt(x * x + y * y);
    if (length > 1.0f) {
        x /= length;
        y /= length;
    }
}

std::uint32_t buttonIf(bool held, InputFrame::Button button)
{
    return held ? static_cast<std::uint32_t>(button) : 0u;
}

}

InputFrame mapKeyboard(const KeyboardState& keys)
{
    InputFrame frame;
    frame.moveX = (keys.right ? 1.0f : 0.0f) - (keys.left ? 1.0f : 0.0f);
    frame.moveY = (keys.up ? 1.0f : 0.0f) - (keys.down ? 1.0f : 0.0f);
    limitToUnitCircle(frame.moveX, frame.moveY);
    frame.buttons = buttonIf(keys.pass, InputFrame::Pass) | buttonIf(keys.through, InputFrame::Through) | buttonIf(keys.shoot, InputFrame::Shoot)
        | buttonIf(keys.sprint, InputFrame::Sprint) | buttonIf(keys.tackle, InputFrame::Tackle) | buttonIf(keys.switchPlayer, InputFrame::Switch)
        | buttonIf(keys.pause, InputFrame::Pause);
    return frame;
}

InputFrame mapGamepad(const GamepadState& pad)
{
    InputFrame frame;
    if (!pad.connected) {
        return frame;
    }
    float x = pad.stickX;
    float y = -pad.stickY;
    const float length = std::sqrt(x * x + y * y);
    if (length < kStickDeadzone) {
        x = 0.0f;
        y = 0.0f;
    } else {
        const float scaled = std::min((length - kStickDeadzone) / (1.0f - kStickDeadzone), 1.0f) / length;
        x *= scaled;
        y *= scaled;
    }
    const float dpadX = (pad.dpadRight ? 1.0f : 0.0f) - (pad.dpadLeft ? 1.0f : 0.0f);
    const float dpadY = (pad.dpadUp ? 1.0f : 0.0f) - (pad.dpadDown ? 1.0f : 0.0f);
    if (dpadX != 0.0f || dpadY != 0.0f) {
        x = dpadX;
        y = dpadY;
    }
    limitToUnitCircle(x, y);
    frame.moveX = x;
    frame.moveY = y;
    frame.buttons = buttonIf(pad.south, InputFrame::Pass) | buttonIf(pad.north, InputFrame::Through) | buttonIf(pad.east, InputFrame::Shoot)
        | buttonIf(pad.west, InputFrame::Tackle) | buttonIf(pad.rightShoulder, InputFrame::Sprint) | buttonIf(pad.leftShoulder, InputFrame::Switch)
        | buttonIf(pad.start, InputFrame::Pause);
    return frame;
}

InputFrame mergeInput(const InputFrame& a, const InputFrame& b)
{
    InputFrame merged;
    merged.buttons = a.buttons | b.buttons;
    const float lengthA = a.moveX * a.moveX + a.moveY * a.moveY;
    const float lengthB = b.moveX * b.moveX + b.moveY * b.moveY;
    const InputFrame& stronger = lengthB > lengthA ? b : a;
    merged.moveX = stronger.moveX;
    merged.moveY = stronger.moveY;
    return merged;
}

}
