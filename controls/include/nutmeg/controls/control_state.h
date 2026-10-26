#pragma once

#include "nutmeg/core/input.h"

#include <array>
#include <cstdint>

namespace nutmeg::controls {

// Turns the held-button bitmask of each InputFrame into press, hold and
// release edges and counts how long each button has been held.
class ControlState {
public:
    static constexpr int kButtonCount = 7;

    void update(const InputFrame& frame);

    bool held(InputFrame::Button button) const { return (held_ & button) != 0; }
    bool pressed(InputFrame::Button button) const { return (pressed_ & button) != 0; }
    bool released(InputFrame::Button button) const { return (released_ & button) != 0; }

    // Ticks the button has been held, counting the tick it went down as 1. Zero when up.
    int heldTicks(InputFrame::Button button) const { return held(button) ? ticks_[index(button)] : 0; }
    // On the tick a button is released: how long it had been held. Otherwise zero.
    int releasedAfterTicks(InputFrame::Button button) const { return released(button) ? ticks_[index(button)] : 0; }

    bool operator==(const ControlState&) const = default;

private:
    static int index(InputFrame::Button button);

    std::uint32_t held_ = 0;
    std::uint32_t pressed_ = 0;
    std::uint32_t released_ = 0;
    std::array<int, kButtonCount> ticks_{};
};

}
