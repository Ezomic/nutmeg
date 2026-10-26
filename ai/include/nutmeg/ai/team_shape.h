#pragma once

#include "nutmeg/ai/formation.h"
#include "nutmeg/ai/tuning.h"
#include "nutmeg/physics/pitch.h"
#include "nutmeg/physics/vec.h"

#include <array>
#include <optional>

namespace nutmeg::ai {

// Which side has the ball, as decided by whoever owns possession.
enum class BallControl { Home, Away, Loose };

enum class Role { Hold, Mark, Press };

struct TeamInput {
    physics::Side side = physics::Side::Home;
    Formation formation = Formation::F442;
    Mentality mentality = Mentality::Balanced;
    BallControl control = BallControl::Loose;
    physics::Vec2 ball; // world coordinates
    // Outfielders in slot order, world coordinates. The keeper is not here.
    std::array<physics::Vec2, kOutfielders> positions{};
    std::array<physics::Vec2, kOutfielders> opponents{};
    // World x of the opponent's offside line; attackers stay level with it.
    std::optional<float> offsideLineX;
};

struct PlayerTarget {
    physics::Vec2 point; // world coordinates
    float urgency = 0.0f;
};

struct TeamIntent {
    Phase phase = Phase::Loose;
    std::array<PlayerTarget, kOutfielders> targets{};
    std::array<Role, kOutfielders> roles{};
};

Phase phaseOf(physics::Side side, BallControl control);

TeamIntent planTeam(const TeamInput& input);

struct ShapeMetrics {
    float lineHeight = 0.0f; // mean local x of the defence-line slots
    float blockLength = 0.0f; // furthest back to furthest forward outfielder
    float blockWidth = 0.0f; // widest to narrowest outfielder
};

// Measured in the local frame, so both sides read alike.
ShapeMetrics measure(physics::Side side, Formation formation, const std::array<physics::Vec2, kOutfielders>& points);

}
