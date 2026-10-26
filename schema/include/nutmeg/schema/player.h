#pragma once

#include "nutmeg/schema/attributes.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace nutmeg::schema {

// Same four codes as Pitch: GK, DF, MF, FW.
enum class Position : std::uint8_t { Goalkeeper, Defender, Midfielder, Forward };

enum class Foot : std::uint8_t { Right, Left, Both };

inline constexpr int minHeightCm = 150;
inline constexpr int maxHeightCm = 210;
inline constexpr int defaultHeightCm = 180;
inline constexpr int minWeightKg = 50;
inline constexpr int maxWeightKg = 110;
inline constexpr int defaultWeightKg = 75;

std::string_view positionCode(Position position);
std::optional<Position> positionFromCode(std::string_view code);
std::string_view footName(Foot foot);
std::optional<Foot> footFromName(std::string_view name);

// Everything the match needs to know about one player. Goalkeeper attributes
// are always present; the engine reads them only when position is Goalkeeper.
struct PlayerProfile {
    std::uint32_t id = 0;
    std::string name;
    Position position = Position::Midfielder;
    Attributes attributes;
    GoalkeeperAttributes goalkeeper;
    Foot foot = Foot::Right;
    int heightCm = defaultHeightCm;
    int weightKg = defaultWeightKg;

    // Every rating clamped to 1..100, height and weight to their ranges.
    PlayerProfile normalised() const;

    bool operator==(const PlayerProfile&) const = default;
};

}

template <>
struct nutmeg::Reflect<nutmeg::schema::PlayerProfile> {
    static constexpr auto fields()
    {
        using nutmeg::schema::PlayerProfile;
        return std::make_tuple(field("id", &PlayerProfile::id), field("name", &PlayerProfile::name),
            field("position", &PlayerProfile::position), field("attributes", &PlayerProfile::attributes),
            field("goalkeeper", &PlayerProfile::goalkeeper), field("foot", &PlayerProfile::foot),
            field("height_cm", &PlayerProfile::heightCm), field("weight_kg", &PlayerProfile::weightKg));
    }
};
