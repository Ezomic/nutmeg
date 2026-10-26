#include "nutmeg/controls/control_state.h"

#include <bit>

namespace nutmeg::controls {

int ControlState::index(InputFrame::Button button)
{
    return std::countr_zero(static_cast<std::uint32_t>(button));
}

void ControlState::update(const InputFrame& frame)
{
    const std::uint32_t before = held_;
    held_ = frame.buttons;
    pressed_ = held_ & ~before;
    released_ = before & ~held_;
    for (int i = 0; i < kButtonCount; ++i) {
        const std::uint32_t bit = 1u << i;
        if ((held_ & bit) != 0) {
            ticks_[i] = (before & bit) != 0 ? ticks_[i] + 1 : 1;
        }
    }
}

}
