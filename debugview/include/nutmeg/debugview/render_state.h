#pragma once

#include <array>
#include <string>

namespace nutmeg::debugview {

// World space: metres, origin at the centre spot, +x towards the right goal,
// +y towards the top touchline.
constexpr float kPitchLength = 105.0f;
constexpr float kPitchWidth = 68.0f;

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct PlayerView {
    Vec2 position;
    float facing = 0.0f; // radians, 0 along +x, counter-clockwise
    int number = 0;
    bool controlled = false;
};

struct BallView {
    Vec2 position;
    float height = 0.0f; // metres above the ground
};

struct RenderState {
    static constexpr int kTeamSize = 11;

    std::array<PlayerView, kTeamSize> home;
    std::array<PlayerView, kTeamSize> away;
    BallView ball;
    std::string overlayText;
};

struct Overlays {
    bool numbers = true;
    bool facing = true;
    bool text = true;
    bool grid = false;
};

}
