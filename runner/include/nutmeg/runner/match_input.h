#pragma once

#include <nlohmann/json.hpp>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace nutmeg::runner {

struct Problem {
    std::string path;
    std::string message;
};

struct PlayerInput {
    std::int64_t id = 0;
    std::string name;
    std::string position;
    std::map<std::string, int> attributes;
};

struct TeamInput {
    std::string name;
    std::string formation;
    std::string mentality;
    std::vector<PlayerInput> lineup;
    std::vector<PlayerInput> bench;
};

struct MatchInput {
    std::string engineVersion;
    std::string matchId;
    bool hasMatchId = false;
    std::uint64_t seed = 0;
    double homeAdvantage = 1.0;
    TeamInput home;
    TeamInput away;
};

// Empty when schemaVersion and engineVersion are acceptable. Otherwise one
// problem per mismatch; a missing or mistyped field is reported by
// parseMatchInput instead, so callers check this first.
std::vector<Problem> checkVersions(const nlohmann::json& doc);

// Fills `input` and returns no problems when the document is valid.
std::vector<Problem> parseMatchInput(const nlohmann::json& doc, MatchInput& input);

}
