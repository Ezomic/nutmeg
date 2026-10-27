#include "nutmeg/gameplay/referee.h"

#include "nutmeg/core/hash.h"
#include "nutmeg/gameplay/actions.h"
#include "nutmeg/physics/pitch.h"

#include <algorithm>

namespace nutmeg::gameplay {

namespace {

using physics::Side;
using physics::Vec2;

namespace tune = referee;

float attackSign(Side side) { return side == Side::Home ? 1.0f : -1.0f; }

Vec2 insidePitch(Vec2 p)
{
    return {std::clamp(p.x, -physics::pitch::kHalfLength, physics::pitch::kHalfLength), std::clamp(p.y, -physics::pitch::kHalfWidth, physics::pitch::kHalfWidth)};
}

}

bool inOffsidePosition(const Player* players, std::size_t count, const Player& attacker, float ballX)
{
    const float sign = attackSign(attacker.side);
    const float line = attacker.position.x * sign;
    if (line <= 0.0f || line <= ballX * sign) {
        return false;
    }
    float last = -1e9f;
    float secondLast = -1e9f;
    for (std::size_t i = 0; i < count; ++i) {
        if (players[i].side == attacker.side || players[i].sentOff) {
            continue;
        }
        const float x = players[i].position.x * sign;
        if (x > last) {
            secondLast = last;
            last = x;
        } else if (x > secondLast) {
            secondLast = x;
        }
    }
    return line > secondLast;
}

Referee::Referee(Rng rng, std::size_t playerCount)
    : rng_(rng)
    , yellows_(playerCount, 0)
    , sentOff_(playerCount, 0)
    , offsideAtPlay_(playerCount, 0)
{
}

std::optional<Restart> Referee::review(const Situation& situation, std::vector<MatchEvent>& events)
{
    std::vector<MatchEvent> rulings;
    std::optional<Restart> restart;

    for (const MatchEvent& event : events) {
        if (restart) {
            break;
        }
        switch (event.kind) {
        case EventKind::FoulCandidate:
            restart = onFoul(event, situation, rulings);
            break;
        case EventKind::Pass:
        case EventKind::Shot:
            markPlayed(event, situation);
            break;
        case EventKind::Touch:
            restart = onTouch(event, situation, rulings);
            break;
        case EventKind::Goal:
            clearPlayed();
            if (advantage_.active) {
                restart = endAdvantage(true, event.tick, situation, rulings);
            }
            break;
        case EventKind::OutOfPlay:
            clearPlayed();
            if (advantage_.active) {
                restart = endAdvantage(false, event.tick, situation, rulings);
            }
            break;
        default:
            break;
        }
    }
    if (!restart && advantage_.active) {
        restart = watchAdvantage(situation, rulings);
    }

    events.insert(events.end(), rulings.begin(), rulings.end());
    return restart;
}

std::optional<Restart> Referee::onFoul(const MatchEvent& event, const Situation& situation, std::vector<MatchEvent>& rulings)
{
    const std::uint32_t fouler = event.player;
    const std::uint32_t victim = event.other;
    if (advantage_.active || fouler >= situation.count || victim >= situation.count || sentOff_[fouler] != 0) {
        return std::nullopt;
    }
    const Player& foulerPlayer = situation.players[fouler];
    const Player& victimPlayer = situation.players[victim];

    const float aggression = skill(foulerPlayer.profile.attributes.aggression);
    const float red = tune::kRedBase + tune::kRedAggression * aggression;
    const float yellow = tune::kYellowBase + tune::kYellowAggression * aggression;
    const float roll = rng_.nextFloat();
    const Card card = roll < red ? Card::Red : (roll < red + yellow ? Card::Yellow : Card::None);

    const Vec2 spot = insidePitch(victimPlayer.position);
    const bool penalty = physics::pitch::inPenaltyArea(spot, foulerPlayer.side);
    Restart restart;
    restart.kind = penalty ? RestartKind::Penalty : RestartKind::DirectFreeKick;
    restart.side = victimPlayer.side;
    restart.spot = penalty ? physics::pitch::penaltySpot(foulerPlayer.side) : spot;

    const float forward = victimPlayer.velocity.x * attackSign(victimPlayer.side);
    const bool advantage = card != Card::Red && !penalty && situation.possessor == victim && forward >= tune::kAdvantageMinForwardSpeed;

    MatchEvent foul;
    foul.kind = EventKind::Foul;
    foul.tick = situation.tick;
    foul.player = fouler;
    foul.other = victim;
    foul.side = foulerPlayer.side;
    foul.value = advantage ? 1.0f : 0.0f;
    rulings.push_back(foul);

    if (!advantage) {
        showCard(fouler, card, situation, rulings);
        return restart;
    }

    foul.kind = EventKind::AdvantageGiven;
    foul.value = 0.0f;
    rulings.push_back(foul);
    advantage_ = {true, fouler, victim, card, restart, situation.tick + tune::kAdvantageTicks};
    return std::nullopt;
}

void Referee::markPlayed(const MatchEvent& event, const Situation& situation)
{
    clearPlayed();
    if (event.player >= situation.count || situation.offsideExempt) {
        return;
    }
    const Player& kicker = situation.players[event.player];
    played_ = true;
    playedBy_ = kicker.id;
    for (std::size_t i = 0; i < situation.count; ++i) {
        const Player& mate = situation.players[i];
        const bool eligible = mate.side == kicker.side && mate.id != kicker.id && !mate.sentOff;
        offsideAtPlay_[i] = eligible && inOffsidePosition(situation.players, situation.count, mate, kicker.position.x) ? 1 : 0;
    }
}

std::optional<Restart> Referee::onTouch(const MatchEvent& event, const Situation& situation, std::vector<MatchEvent>& rulings)
{
    if (!played_) {
        return std::nullopt;
    }
    const bool offside = event.player < situation.count && offsideAtPlay_[event.player] != 0;
    const std::uint32_t kicker = playedBy_;
    clearPlayed();
    if (!offside) {
        return std::nullopt;
    }

    const Player& attacker = situation.players[event.player];
    MatchEvent ruling;
    ruling.kind = EventKind::Offside;
    ruling.tick = situation.tick;
    ruling.player = attacker.id;
    ruling.other = kicker;
    ruling.side = attacker.side;
    rulings.push_back(ruling);

    Restart restart;
    restart.kind = RestartKind::IndirectFreeKick;
    restart.side = physics::opposite(attacker.side);
    restart.spot = insidePitch(attacker.position);
    return restart;
}

std::optional<Restart> Referee::watchAdvantage(const Situation& situation, std::vector<MatchEvent>& rulings)
{
    const bool lost = situation.possessor < situation.count && situation.players[situation.possessor].side == situation.players[advantage_.fouler].side;
    if (lost) {
        return endAdvantage(false, situation.tick, situation, rulings);
    }
    if (situation.tick >= advantage_.expires) {
        return endAdvantage(true, situation.tick, situation, rulings);
    }
    return std::nullopt;
}

std::optional<Restart> Referee::endAdvantage(bool kept, std::uint64_t tick, const Situation& situation, std::vector<MatchEvent>& rulings)
{
    const Advantage ended = advantage_;
    advantage_ = {};

    MatchEvent ruling;
    ruling.kind = EventKind::AdvantageEnded;
    ruling.tick = tick;
    ruling.player = ended.fouler;
    ruling.other = ended.victim;
    ruling.side = situation.players[ended.fouler].side;
    ruling.value = kept ? 1.0f : 0.0f;
    rulings.push_back(ruling);

    showCard(ended.fouler, ended.card, situation, rulings);
    if (kept) {
        return std::nullopt;
    }
    return ended.restart;
}

void Referee::showCard(std::uint32_t player, Card card, const Situation& situation, std::vector<MatchEvent>& rulings)
{
    if (card == Card::None) {
        return;
    }
    MatchEvent ruling;
    ruling.tick = situation.tick;
    ruling.player = player;
    ruling.side = situation.players[player].side;

    const bool straightRed = card == Card::Red;
    if (card == Card::Yellow) {
        ++yellows_[player];
        ruling.kind = EventKind::YellowCard;
        ruling.value = static_cast<float>(yellows_[player]);
        rulings.push_back(ruling);
    }
    if (straightRed || yellows_[player] >= 2) {
        sentOff_[player] = 1;
        ruling.kind = EventKind::RedCard;
        ruling.value = straightRed ? 1.0f : 2.0f;
        rulings.push_back(ruling);
    }
}

void Referee::clearPlayed()
{
    played_ = false;
    playedBy_ = kNoPlayer;
    std::fill(offsideAtPlay_.begin(), offsideAtPlay_.end(), std::uint8_t{0});
}

std::uint64_t Referee::hash() const
{
    std::uint64_t h = mix64(rng_.state());
    for (std::size_t i = 0; i < yellows_.size(); ++i) {
        h = hashCombine(h, mix64(yellows_[i] | (static_cast<std::uint64_t>(sentOff_[i]) << 8) | (static_cast<std::uint64_t>(offsideAtPlay_[i]) << 16)));
    }
    h = hashCombine(h, mix64((played_ ? 1u : 0u) | (static_cast<std::uint64_t>(playedBy_) << 1)));
    h = hashCombine(h, mix64((advantage_.active ? 1u : 0u) | (static_cast<std::uint64_t>(advantage_.fouler) << 1)));
    h = hashCombine(h, mix64(advantage_.expires));
    return h;
}

}
