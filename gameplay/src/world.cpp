#include "nutmeg/gameplay/world.h"

#include "nutmeg/core/dmath.h"
#include "nutmeg/core/hash.h"
#include "nutmeg/gameplay/locomotion.h"

namespace nutmeg::gameplay {

namespace {

using physics::Side;
using physics::Vec2;

// 4-4-2 for Home as {x, y} on the Home half; Away is mirrored through the centre spot.
constexpr std::array<Vec2, kPlayersPerSide> kFormation = {{
    {-50.0f, 0.0f},
    {-35.0f, -24.0f},
    {-35.0f, -8.0f},
    {-35.0f, 8.0f},
    {-35.0f, 24.0f},
    {-20.0f, -24.0f},
    {-20.0f, -8.0f},
    {-20.0f, 8.0f},
    {-20.0f, 24.0f},
    {-6.0f, -5.0f},
    {-6.0f, 5.0f},
}};

Vec2 intentOf(const InputFrame& input, Side side)
{
    const float sign = side == Side::Home ? 1.0f : -1.0f;
    return {input.moveY * sign, -input.moveX * sign};
}

void separate(std::array<Player, kPlayerCount>& players)
{
    constexpr float minDistance = 2.0f * locomotion::kBodyRadius;
    for (int pass = 0; pass < locomotion::kSeparationPasses; ++pass) {
        for (std::size_t i = 0; i < kPlayerCount; ++i) {
            for (std::size_t j = i + 1; j < kPlayerCount; ++j) {
                const Vec2 d = players[j].position - players[i].position;
                const float dist = physics::length(d);
                if (dist >= minDistance) {
                    continue;
                }
                const Vec2 normal = dist > 1e-6f ? d * (1.0f / dist) : Vec2{1.0f, 0.0f};
                const Vec2 push = normal * ((minDistance - dist) * 0.5f);
                players[i].position = players[i].position - push;
                players[j].position = players[j].position + push;
            }
        }
    }
}

void clampToPitch(Player& p)
{
    constexpr float maxX = physics::pitch::kHalfLength + locomotion::kPitchMargin;
    constexpr float maxY = physics::pitch::kHalfWidth + locomotion::kPitchMargin;
    if (p.position.x > maxX) {
        p.position.x = maxX;
        p.velocity.x = p.velocity.x > 0.0f ? 0.0f : p.velocity.x;
    } else if (p.position.x < -maxX) {
        p.position.x = -maxX;
        p.velocity.x = p.velocity.x < 0.0f ? 0.0f : p.velocity.x;
    }
    if (p.position.y > maxY) {
        p.position.y = maxY;
        p.velocity.y = p.velocity.y > 0.0f ? 0.0f : p.velocity.y;
    } else if (p.position.y < -maxY) {
        p.position.y = -maxY;
        p.velocity.y = p.velocity.y < 0.0f ? 0.0f : p.velocity.y;
    }
}

std::array<schema::PlayerProfile, kPlayerCount> defaultProfiles()
{
    std::array<schema::PlayerProfile, kPlayerCount> profiles;
    for (std::size_t i = 0; i < kPlayerCount; ++i) {
        profiles[i].id = static_cast<std::uint32_t>(i);
        profiles[i].position = i % kPlayersPerSide == 0 ? schema::Position::Goalkeeper : schema::Position::Midfielder;
    }
    return profiles;
}

}

World::World()
    : World(defaultProfiles())
{
}

World::World(const std::array<schema::PlayerProfile, kPlayerCount>& profiles)
{
    for (std::size_t i = 0; i < kPlayerCount; ++i) {
        Player& p = players_[i];
        const bool home = i < kPlayersPerSide;
        const Vec2 slot = kFormation[i % kPlayersPerSide];
        p.id = static_cast<std::uint32_t>(i);
        p.side = home ? Side::Home : Side::Away;
        p.position = home ? slot : Vec2{-slot.x, -slot.y};
        p.facing = home ? 0.0f : dmath::kPi;
        p.profile = profiles[i];
    }
}

void World::step(const PlayerInputs& inputs)
{
    for (std::size_t i = 0; i < kPlayerCount; ++i) {
        Player& p = players_[i];
        const bool sprint = (inputs[i].buttons & InputFrame::Sprint) != 0;
        locomotion::step(p, intentOf(inputs[i], p.side), sprint);
    }
    separate(players_);
    for (Player& p : players_) {
        clampToPitch(p);
    }
    ball_.step();
    ++tick_;
}

std::uint64_t World::hash() const
{
    std::uint64_t h = mix64(tick_);
    for (const Player& p : players_) {
        h = hashCombine(h, p.hash());
    }
    return hashCombine(h, ball_.hash());
}

}
