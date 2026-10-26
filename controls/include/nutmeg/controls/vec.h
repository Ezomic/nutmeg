#pragma once

namespace nutmeg::controls {

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;

    constexpr bool operator==(const Vec2&) const = default;
};

}
