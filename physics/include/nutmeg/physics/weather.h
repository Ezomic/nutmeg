#pragma once

#include "nutmeg/physics/vec.h"

namespace nutmeg::physics {

// Per-fixture conditions. The default is clear, still air and a dry pitch, and
// reproduces the unweathered ball physics bit for bit.
struct Weather {
    Vec2 wind;           // m/s, the velocity of the air over the ground
    float rain = 0.0f;   // 0 dry to 1 downpour
    float wetPitch = 0.0f; // 0 dry to 1 waterlogged

    constexpr bool operator==(const Weather&) const = default;
};

namespace weather {

constexpr float kWetRollDecelReduction = 0.4f;
constexpr float kWetTangentKeepGain = 0.12f;
constexpr float kRainRestitutionLoss = 0.06f;

}

}
