#pragma once

#include "nutmeg/ai/tuning.h"
#include "nutmeg/physics/vec.h"

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>

namespace nutmeg::ai {

inline constexpr std::size_t kOutfielders = 10;

enum class Formation { F442, F433, F352, F4231, F532, F343 };

enum class Line { Defence, Midfield, Attack };

struct Slot {
    physics::Vec2 anchor;
    Line line = Line::Midfield;
};

// Ten outfield slots in the local frame: the team attacks +x from its own half,
// y is positive towards the left of an attacker. Anchors are the resting shape
// with the ball on the halfway line.
const std::array<Slot, kOutfielders>& slots(Formation formation);

std::optional<Formation> parseFormation(std::string_view name);
std::optional<Mentality> parseMentality(std::string_view name);

}
