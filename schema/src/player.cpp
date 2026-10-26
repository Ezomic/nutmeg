#include "nutmeg/schema/player.h"

#include <algorithm>

namespace nutmeg::schema {

std::string_view positionCode(Position position)
{
    switch (position) {
    case Position::Goalkeeper: return "GK";
    case Position::Defender: return "DF";
    case Position::Midfielder: return "MF";
    case Position::Forward: return "FW";
    }
    return "";
}

std::optional<Position> positionFromCode(std::string_view code)
{
    for (const auto position : {Position::Goalkeeper, Position::Defender, Position::Midfielder, Position::Forward}) {
        if (positionCode(position) == code) return position;
    }
    return std::nullopt;
}

std::string_view footName(Foot foot)
{
    switch (foot) {
    case Foot::Right: return "right";
    case Foot::Left: return "left";
    case Foot::Both: return "both";
    }
    return "";
}

std::optional<Foot> footFromName(std::string_view name)
{
    for (const auto foot : {Foot::Right, Foot::Left, Foot::Both}) {
        if (footName(foot) == name) return foot;
    }
    return std::nullopt;
}

PlayerProfile PlayerProfile::normalised() const
{
    PlayerProfile result = *this;
    result.attributes = attributes.clamped();
    result.goalkeeper = goalkeeper.clamped();
    result.heightCm = std::clamp(heightCm, minHeightCm, maxHeightCm);
    result.weightKg = std::clamp(weightKg, minWeightKg, maxWeightKg);
    return result;
}

}
