#include "nutmeg/controls/stick.h"

#include <array>
#include <cmath>

namespace nutmeg::controls {

namespace {

constexpr float kDiagonal = 0.70710678f;

constexpr std::array<Vec2, 8> kCompass = {{
    {1.0f, 0.0f},
    {kDiagonal, kDiagonal},
    {0.0f, 1.0f},
    {-kDiagonal, kDiagonal},
    {-1.0f, 0.0f},
    {-kDiagonal, -kDiagonal},
    {0.0f, -1.0f},
    {kDiagonal, -kDiagonal},
}};

// The compass point with the largest dot product is the nearest in angle.
// Comparing dot products keeps trigonometry, whose results differ between
// libraries, out of the simulation path.
Vec2 nearestCompassPoint(Vec2 v)
{
    Vec2 best = kCompass[0];
    float bestDot = v.x * best.x + v.y * best.y;
    for (std::size_t i = 1; i < kCompass.size(); ++i) {
        const float d = v.x * kCompass[i].x + v.y * kCompass[i].y;
        if (d > bestDot) {
            bestDot = d;
            best = kCompass[i];
        }
    }
    return best;
}

}

Vec2 processStick(Vec2 raw, const StickConfig& config)
{
    const float length = std::sqrt(raw.x * raw.x + raw.y * raw.y);
    if (length <= config.deadzone || length <= 0.0f) {
        return {};
    }
    const float span = 1.0f - config.deadzone;
    float magnitude = span > 0.0f ? (length - config.deadzone) / span : 1.0f;
    if (magnitude > 1.0f) {
        magnitude = 1.0f;
    }
    Vec2 direction{raw.x / length, raw.y / length};
    if (config.snapToEightWay) {
        direction = nearestCompassPoint(direction);
    }
    return {direction.x * magnitude, direction.y * magnitude};
}

}
