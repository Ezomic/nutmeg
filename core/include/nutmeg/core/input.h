#pragma once

#include "nutmeg/core/reflect.h"

#include <cstdint>

namespace nutmeg {

// One tick of player input. The simulation never reads a device; it reads
// these, which is what makes live play and replay the same code path.
struct InputFrame {
    enum Button : std::uint32_t {
        Pass = 1u << 0,
        Through = 1u << 1,
        Shoot = 1u << 2,
        Sprint = 1u << 3,
        Tackle = 1u << 4,
        Switch = 1u << 5,
        Pause = 1u << 6,
    };
    std::uint32_t buttons = 0;
    float moveX = 0.0f; // towards the right touchline
    float moveY = 0.0f; // towards the opponent's goal

    bool operator==(const InputFrame&) const = default;
};

template <>
struct Reflect<InputFrame> {
    static constexpr auto fields()
    {
        return std::make_tuple(field("buttons", &InputFrame::buttons), field("moveX", &InputFrame::moveX), field("moveY", &InputFrame::moveY));
    }
};

}
