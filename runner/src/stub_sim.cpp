#include "nutmeg/runner/stub_sim.h"

#include "nutmeg/core/rng.h"
#include "nutmeg/runner/version.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace nutmeg::runner {

namespace {

using json = nlohmann::json;

struct Counters {
    int minutes = 90;
    int goals = 0;
    int shots = 0;
    int passes = 0;
    int passesCompleted = 0;
    int crosses = 0;
    int tackles = 0;
    int interceptions = 0;
    int clearances = 0;
    int fouls = 0;
    int saves = 0;
    int yellowCards = 0;
};

struct TeamTally {
    int shots = 0;
    int onTarget = 0;
    int goals = 0;
    int passes = 0;
    int passesCompleted = 0;
    int crosses = 0;
    int fouls = 0;
    int corners = 0;
    int offsides = 0;
    int yellowCards = 0;
    int tackles = 0;
    int saves = 0;
    int possessionTenths = 500;
};

struct Side {
    const TeamInput* input = nullptr;
    const char* label = "";
    bool home = false;
    double overall = 0.0;
    TeamTally tally;
    std::vector<Counters> players;
};

struct Event {
    int minute = 0;
    const char* type = "";
    const Side* side = nullptr;
    std::int64_t playerId = 0;
    std::int64_t otherPlayerId = 0;
    const char* outcome = nullptr;
    double x = 0.0;
    double y = 0.0;
};

// Mirrors Pitch's Mentality::attackBias() and defenceBias().
double attackBias(const std::string& mentality)
{
    return mentality == "attacking" ? 1.10 : mentality == "defensive" ? 0.90 : 1.0;
}

double defenceBias(const std::string& mentality)
{
    return mentality == "attacking" ? 0.85 : mentality == "defensive" ? 1.15 : 1.0;
}

double overall(const TeamInput& team)
{
    double sum = 0.0;
    for (const auto& player : team.lineup) {
        int total = 0;
        for (const char* name : {"vision", "passing", "dribbling", "finishing", "tackling", "pace"}) {
            total += player.attributes.at(name);
        }
        sum += total / 6.0;
    }
    return sum / static_cast<double>(team.lineup.size());
}

double round1(double value)
{
    return std::round(value * 10.0) / 10.0;
}

class Stub {
public:
    explicit Stub(const MatchInput& input)
        : input_(input)
        , rng_(input.seed)
    {
        sides_[0] = makeSide(input.home, "home", true);
        sides_[1] = makeSide(input.away, "away", false);
    }

    json run()
    {
        for (int i = 0; i < 2; ++i) {
            shotsFor(sides_[i], sides_[1 - i]);
        }
        for (int i = 0; i < 2; ++i) {
            incidentsFor(sides_[i], sides_[1 - i]);
            aggregatesFor(sides_[i]);
        }
        possession();
        std::stable_sort(events_.begin(), events_.end(), [](const Event& a, const Event& b) { return a.minute < b.minute; });
        return result();
    }

private:
    static Side makeSide(const TeamInput& team, const char* label, bool home)
    {
        Side side;
        side.input = &team;
        side.label = label;
        side.home = home;
        side.overall = overall(team);
        side.players.resize(team.lineup.size());
        return side;
    }

    std::size_t pick(const Side& side, const std::array<int, 4>& weightByPosition)
    {
        const std::array<std::string, 4> positions = {"GK", "DF", "MF", "FW"};
        std::uint32_t total = 0;
        for (const auto& player : side.input->lineup) {
            total += static_cast<std::uint32_t>(weightByPosition[std::find(positions.begin(), positions.end(), player.position) - positions.begin()]);
        }
        if (total == 0) {
            return rng_.range(0, static_cast<std::uint32_t>(side.input->lineup.size() - 1));
        }
        std::uint32_t roll = rng_.range(0, total - 1);
        for (std::size_t i = 0; i < side.input->lineup.size(); ++i) {
            const auto& player = side.input->lineup[i];
            const auto weight = static_cast<std::uint32_t>(weightByPosition[std::find(positions.begin(), positions.end(), player.position) - positions.begin()]);
            if (roll < weight) {
                return i;
            }
            roll -= weight;
        }
        return 0;
    }

    std::size_t keeper(const Side& side) const
    {
        for (std::size_t i = 0; i < side.input->lineup.size(); ++i) {
            if (side.input->lineup[i].position == "GK") {
                return i;
            }
        }
        return 0;
    }

    int minute() { return static_cast<int>(rng_.range(1, 90)); }

    double unit() { return static_cast<double>(rng_.nextFloat()); }

    // x runs from the home goal line (0) to the away goal line (1), as in
    // Pitch's MatchLineups, so an away attack is the home frame mirrored.
    double attackX(const Side& side, double depth) const { return round3(side.home ? depth : 1.0 - depth); }

    static double round3(double v) { return std::round(v * 1000.0) / 1000.0; }

    std::int64_t idOf(const Side& side, std::size_t index) const { return side.input->lineup[index].id; }

    void shotsFor(Side& attack, Side& defence)
    {
        const double advantage = attack.home ? input_.homeAdvantage : 1.0 / input_.homeAdvantage;
        const double factor = attack.overall / defence.overall * attackBias(attack.input->mentality)
            * defenceBias(defence.input->mentality) * advantage;
        const double goalChance = std::clamp(0.10 * factor, 0.02, 0.50);
        const int shots = static_cast<int>(rng_.range(6, 16));

        for (int s = 0; s < shots; ++s) {
            const int at = minute();
            const std::size_t shooter = pick(attack, {0, 1, 3, 5});
            const double x = attackX(attack, 0.72 + 0.23 * unit());
            const double y = round3(0.2 + 0.6 * unit());
            const bool goal = unit() < goalChance;
            const double kind = unit();
            const char* outcome = goal ? "goal" : kind < 0.5 ? "saved" : kind < 0.7 ? "blocked" : "off";

            attack.players[shooter].shots++;
            attack.tally.shots++;
            Event shot{at, goal ? "goal" : "shot", &attack, idOf(attack, shooter), 0, outcome, x, y};

            if (goal) {
                attack.players[shooter].goals++;
                attack.tally.goals++;
                attack.tally.onTarget++;
                if (unit() < 0.7) {
                    const std::size_t helper = pick(attack, {0, 2, 4, 3});
                    shot.otherPlayerId = helper == shooter ? 0 : idOf(attack, helper);
                }
            } else if (std::string(outcome) == "saved") {
                attack.tally.onTarget++;
                const std::size_t gk = keeper(defence);
                defence.players[gk].saves++;
                defence.tally.saves++;
                events_.push_back({at, "save", &defence, idOf(defence, gk), idOf(attack, shooter), nullptr, defence.home ? 0.04 : 0.96, 0.5});
            }
            events_.push_back(shot);
        }
    }

    void incidentsFor(Side& side, Side& other)
    {
        const int fouls = static_cast<int>(rng_.range(6, 16));
        for (int i = 0; i < fouls; ++i) {
            const int at = minute();
            const std::size_t culprit = pick(side, {0, 4, 4, 3});
            const std::size_t victim = pick(other, {0, 3, 4, 4});
            const double x = round3(0.15 + 0.7 * unit());
            const double y = round3(0.05 + 0.9 * unit());
            side.players[culprit].fouls++;
            side.tally.fouls++;
            events_.push_back({at, "foul", &side, idOf(side, culprit), idOf(other, victim), nullptr, x, y});
            if (unit() < 0.12 && side.players[culprit].yellowCards == 0) {
                side.players[culprit].yellowCards++;
                side.tally.yellowCards++;
                events_.push_back({at, "yellow_card", &side, idOf(side, culprit), 0, nullptr, x, y});
            }
        }

        const int corners = static_cast<int>(rng_.range(2, 9));
        for (int i = 0; i < corners; ++i) {
            const std::size_t taker = pick(side, {0, 1, 4, 2});
            side.tally.corners++;
            events_.push_back({minute(), "corner", &side, idOf(side, taker), 0, nullptr, side.home ? 1.0 : 0.0, unit() < 0.5 ? 0.0 : 1.0});
        }

        const int offsides = static_cast<int>(rng_.range(0, 4));
        for (int i = 0; i < offsides; ++i) {
            const std::size_t offender = pick(side, {0, 0, 1, 6});
            side.tally.offsides++;
            events_.push_back({minute(), "offside", &side, idOf(side, offender), 0, nullptr, attackX(side, 0.7 + 0.25 * unit()), round3(0.1 + 0.8 * unit())});
        }
    }

    void aggregatesFor(Side& side)
    {
        side.tally.passes = static_cast<int>(rng_.range(280, 520));
        side.tally.passesCompleted = side.tally.passes * (60 + static_cast<int>(side.overall / 5.0)) / 100;
        side.tally.crosses = static_cast<int>(rng_.range(5, 25));
        side.tally.tackles = static_cast<int>(rng_.range(10, 30));

        for (int i = 0; i < side.tally.passes; ++i) {
            const std::size_t who = pick(side, {2, 4, 5, 3});
            side.players[who].passes++;
            if (i < side.tally.passesCompleted) {
                side.players[who].passesCompleted++;
            }
        }
        for (int i = 0; i < side.tally.crosses; ++i) {
            side.players[pick(side, {0, 3, 4, 3})].crosses++;
        }
        for (int i = 0; i < side.tally.tackles; ++i) {
            side.players[pick(side, {0, 6, 4, 1})].tackles++;
        }
        const int interceptions = static_cast<int>(rng_.range(6, 18));
        for (int i = 0; i < interceptions; ++i) {
            side.players[pick(side, {0, 6, 4, 1})].interceptions++;
        }
        const int clearances = static_cast<int>(rng_.range(4, 20));
        for (int i = 0; i < clearances; ++i) {
            side.players[pick(side, {0, 8, 2, 0})].clearances++;
        }
    }

    void possession()
    {
        const double lean = (sides_[0].overall - sides_[1].overall) * 0.8 + (static_cast<int>(rng_.range(0, 60)) - 30) / 10.0;
        const int home = static_cast<int>(std::lround(std::clamp(500.0 + lean * 10.0, 300.0, 700.0)));
        sides_[0].tally.possessionTenths = home;
        sides_[1].tally.possessionTenths = 1000 - home;
    }

    static double rate(const Counters& c)
    {
        const double rating = 6.0 + 1.3 * c.goals + 0.12 * c.shots + 0.06 * c.crosses + 0.12 * c.tackles
            + 0.08 * c.interceptions + 0.05 * c.clearances + 0.22 * c.saves - 0.15 * c.fouls - 0.3 * c.yellowCards;
        return round1(std::clamp(rating, 1.0, 10.0));
    }

    static json teamJson(const TeamTally& t)
    {
        return {
            {"possession", t.possessionTenths / 10.0},
            {"shots", t.shots},
            {"onTarget", t.onTarget},
            {"goals", t.goals},
            {"passes", t.passes},
            {"passesCompleted", t.passesCompleted},
            {"crosses", t.crosses},
            {"fouls", t.fouls},
            {"corners", t.corners},
            {"offsides", t.offsides},
            {"yellowCards", t.yellowCards},
            {"redCards", 0},
            {"tackles", t.tackles},
            {"saves", t.saves},
        };
    }

    json result() const
    {
        json players = json::array();
        for (const Side& side : sides_) {
            for (std::size_t i = 0; i < side.players.size(); ++i) {
                const Counters& c = side.players[i];
                players.push_back({
                    {"id", side.input->lineup[i].id},
                    {"team", side.label},
                    {"minutes", c.minutes},
                    {"rating", rate(c)},
                    {"goals", c.goals},
                    {"shots", c.shots},
                    {"passes", c.passes},
                    {"passesCompleted", c.passesCompleted},
                    {"crosses", c.crosses},
                    {"tackles", c.tackles},
                    {"interceptions", c.interceptions},
                    {"clearances", c.clearances},
                    {"fouls", c.fouls},
                    {"saves", c.saves},
                });
            }
        }

        json events = json::array();
        for (const Event& e : events_) {
            json item = {
                {"minute", e.minute},
                {"type", e.type},
                {"team", e.side->label},
                {"playerId", e.playerId},
                {"x", e.x},
                {"y", e.y},
            };
            if (e.otherPlayerId != 0) {
                item["otherPlayerId"] = e.otherPlayerId;
            }
            if (e.outcome != nullptr) {
                item["outcome"] = e.outcome;
            }
            events.push_back(std::move(item));
        }

        json out = {
            {"schemaVersion", kSchemaVersion},
            {"engineVersion", kEngineVersion},
            {"stub", true},
            {"seed", input_.seed},
            {"score", {{"home", sides_[0].tally.goals}, {"away", sides_[1].tally.goals}}},
            {"teams", {{"home", teamJson(sides_[0].tally)}, {"away", teamJson(sides_[1].tally)}}},
            {"players", std::move(players)},
            {"events", std::move(events)},
        };
        if (input_.hasMatchId) {
            out["matchId"] = input_.matchId;
        }
        return out;
    }

    const MatchInput& input_;
    Rng rng_;
    std::array<Side, 2> sides_;
    std::vector<Event> events_;
};

}

json runStubSimulation(const MatchInput& input)
{
    return Stub(input).run();
}

}
