#include "nutmeg/ai/formation.h"

namespace nutmeg::ai {

namespace {

using physics::Vec2;

constexpr Slot D(float x, float y) { return {{x, y}, Line::Defence}; }
constexpr Slot M(float x, float y) { return {{x, y}, Line::Midfield}; }
constexpr Slot A(float x, float y) { return {{x, y}, Line::Attack}; }

constexpr std::array<Slot, kOutfielders> k442 = {{
    D(-34, -24), D(-36, -8), D(-36, 8), D(-34, 24),
    M(-20, -24), M(-20, -8), M(-20, 8), M(-20, 24),
    A(-6, -8), A(-6, 8),
}};

constexpr std::array<Slot, kOutfielders> k433 = {{
    D(-34, -24), D(-36, -8), D(-36, 8), D(-34, 24),
    M(-22, -14), M(-18, 0), M(-22, 14),
    A(-8, -22), A(-5, 0), A(-8, 22),
}};

constexpr std::array<Slot, kOutfielders> k352 = {{
    D(-37, -14), D(-38, 0), D(-37, 14),
    M(-24, -28), M(-21, -12), M(-19, 0), M(-21, 12), M(-24, 28),
    A(-6, -8), A(-6, 8),
}};

constexpr std::array<Slot, kOutfielders> k4231 = {{
    D(-34, -24), D(-36, -8), D(-36, 8), D(-34, 24),
    M(-24, -9), M(-24, 9),
    M(-13, -20), M(-12, 0), M(-13, 20),
    A(-5, 0),
}};

constexpr std::array<Slot, kOutfielders> k532 = {{
    D(-30, -28), D(-37, -14), D(-38, 0), D(-37, 14), D(-30, 28),
    M(-20, -14), M(-18, 0), M(-20, 14),
    A(-6, -8), A(-6, 8),
}};

constexpr std::array<Slot, kOutfielders> k343 = {{
    D(-37, -14), D(-38, 0), D(-37, 14),
    M(-22, -24), M(-20, -8), M(-20, 8), M(-22, 24),
    A(-8, -18), A(-5, 0), A(-8, 18),
}};

}

const std::array<Slot, kOutfielders>& slots(Formation formation)
{
    switch (formation) {
    case Formation::F442: return k442;
    case Formation::F433: return k433;
    case Formation::F352: return k352;
    case Formation::F4231: return k4231;
    case Formation::F532: return k532;
    case Formation::F343: return k343;
    }
    return k442;
}

std::optional<Formation> parseFormation(std::string_view name)
{
    if (name == "442") return Formation::F442;
    if (name == "433") return Formation::F433;
    if (name == "352") return Formation::F352;
    if (name == "4231") return Formation::F4231;
    if (name == "532") return Formation::F532;
    if (name == "343") return Formation::F343;
    return std::nullopt;
}

std::optional<Mentality> parseMentality(std::string_view name)
{
    if (name == "attacking") return Mentality::Attacking;
    if (name == "balanced") return Mentality::Balanced;
    if (name == "defensive") return Mentality::Defensive;
    return std::nullopt;
}

}
