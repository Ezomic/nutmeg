#pragma once

#include "nutmeg/controls/vec.h"

namespace nutmeg::controls {

struct StickConfig {
    float deadzone = 0.2f;
    bool snapToEightWay = false;
};

// Radial deadzone: inside it the result is zero, outside it the magnitude is
// rescaled so the output rises smoothly from 0 at the edge to 1 at full tilt.
// The result never leaves the unit circle. With snapToEightWay the direction
// is rounded to the nearest of the eight compass points, which is what a
// keyboard or d-pad should feel like.
Vec2 processStick(Vec2 raw, const StickConfig& config = {});

}
