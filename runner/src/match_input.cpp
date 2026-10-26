#include "nutmeg/runner/match_input.h"

#include "nutmeg/runner/version.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <set>
#include <string_view>

namespace nutmeg::runner {

namespace {

using json = nlohmann::json;

constexpr std::uint64_t kMaxSeed = 9007199254740991ull;
constexpr std::array<std::string_view, 6> kFormations = {"442", "433", "352", "4231", "532", "343"};
constexpr std::array<std::string_view, 3> kMentalities = {"attacking", "balanced", "defensive"};
constexpr std::array<std::string_view, 4> kPositions = {"GK", "DF", "MF", "FW"};
constexpr std::array<std::string_view, 6> kCoreAttributes = {"vision", "passing", "dribbling", "finishing", "tackling", "pace"};

template <std::size_t N>
bool oneOf(const std::string& value, const std::array<std::string_view, N>& allowed)
{
    return std::find(allowed.begin(), allowed.end(), value) != allowed.end();
}

bool isInteger(const json& v)
{
    return v.is_number_integer();
}

bool integerIn(const json& v, std::int64_t min, std::int64_t max)
{
    if (!isInteger(v)) {
        return false;
    }
    if (v.is_number_unsigned() && v.get<std::uint64_t>() > static_cast<std::uint64_t>(max)) {
        return false;
    }
    return v.is_number_unsigned() || (v.get<std::int64_t>() >= min && v.get<std::int64_t>() <= max);
}

struct Parser {
    std::vector<Problem> problems;

    void fail(const std::string& path, const std::string& message) { problems.push_back({path, message}); }

    const json* field(const json& obj, const std::string& path, const char* key, bool required)
    {
        const auto it = obj.find(key);
        if (it == obj.end()) {
            if (required) {
                fail(path + "/" + key, "is required");
            }
            return nullptr;
        }
        return &*it;
    }

    bool str(const json& obj, const std::string& path, const char* key, std::string& out)
    {
        const json* v = field(obj, path, key, true);
        if (v == nullptr) {
            return false;
        }
        if (!v->is_string() || v->get<std::string>().empty() || v->get<std::string>().size() > 128) {
            fail(path + "/" + key, "must be a non-empty string of at most 128 bytes");
            return false;
        }
        out = v->get<std::string>();
        return true;
    }

    template <std::size_t N>
    void choice(const json& obj, const std::string& path, const char* key, const std::array<std::string_view, N>& allowed, std::string& out)
    {
        std::string value;
        if (!str(obj, path, key, value)) {
            return;
        }
        if (!oneOf(value, allowed)) {
            std::string list;
            for (const auto a : allowed) {
                list += (list.empty() ? "" : ", ") + std::string(a);
            }
            fail(path + "/" + key, "must be one of " + list);
            return;
        }
        out = value;
    }

    void attributes(const json& obj, const std::string& path, PlayerInput& player)
    {
        const json* attrs = field(obj, path, "attributes", true);
        if (attrs == nullptr) {
            return;
        }
        if (!attrs->is_object()) {
            fail(path + "/attributes", "must be an object");
            return;
        }
        for (const auto name : kCoreAttributes) {
            if (!attrs->contains(std::string(name))) {
                fail(path + "/attributes/" + std::string(name), "is required");
            }
        }
        for (const auto& [name, value] : attrs->items()) {
            if (!integerIn(value, 1, 100)) {
                fail(path + "/attributes/" + name, "must be an integer from 1 to 100");
                continue;
            }
            player.attributes[name] = value.get<int>();
        }
    }

    void optionalInteger(const json& obj, const std::string& path, const char* key, std::int64_t min, std::int64_t max)
    {
        const json* v = field(obj, path, key, false);
        if (v != nullptr && !integerIn(*v, min, max)) {
            fail(path + "/" + key, "must be an integer from " + std::to_string(min) + " to " + std::to_string(max));
        }
    }

    void player(const json& v, const std::string& path, PlayerInput& out)
    {
        if (!v.is_object()) {
            fail(path, "must be an object");
            return;
        }
        if (const json* id = field(v, path, "id", true)) {
            if (!integerIn(*id, 1, 9007199254740991)) {
                fail(path + "/id", "must be an integer of at least 1");
            } else {
                out.id = id->get<std::int64_t>();
            }
        }
        str(v, path, "name", out.name);
        choice(v, path, "position", kPositions, out.position);
        optionalInteger(v, path, "slot", 0, 10);
        optionalInteger(v, path, "shirt", 1, 99);
        attributes(v, path, out);
    }

    void squad(const json& obj, const std::string& path, const char* key, std::vector<PlayerInput>& out, std::size_t min, std::size_t max)
    {
        const json* list = field(obj, path, key, min > 0);
        if (list == nullptr) {
            return;
        }
        const std::string where = path + "/" + key;
        if (!list->is_array() || list->size() < min || list->size() > max) {
            const std::string count = min == max ? std::to_string(min) : std::to_string(min) + " to " + std::to_string(max);
            fail(where, "must be an array of " + count + " players");
            return;
        }
        out.resize(list->size());
        for (std::size_t i = 0; i < list->size(); ++i) {
            player((*list)[i], where + "/" + std::to_string(i), out[i]);
        }
    }

    void team(const json& v, const std::string& path, TeamInput& out)
    {
        if (!v.is_object()) {
            fail(path, "must be an object");
            return;
        }
        str(v, path, "name", out.name);
        choice(v, path, "formation", kFormations, out.formation);
        choice(v, path, "mentality", kMentalities, out.mentality);
        squad(v, path, "lineup", out.lineup, 11, 11);
        squad(v, path, "bench", out.bench, 0, 12);

        const auto keepers = std::count_if(out.lineup.begin(), out.lineup.end(), [](const PlayerInput& p) { return p.position == "GK"; });
        if (out.lineup.size() == 11 && keepers != 1) {
            fail(path + "/lineup", "must contain exactly one GK");
        }
    }

    void uniqueIds(const MatchInput& input)
    {
        std::set<std::int64_t> seen;
        for (const TeamInput* team : {&input.home, &input.away}) {
            const std::string side = team == &input.home ? "/home" : "/away";
            for (const auto* group : {&team->lineup, &team->bench}) {
                const std::string list = group == &team->lineup ? "/lineup/" : "/bench/";
                for (std::size_t i = 0; i < group->size(); ++i) {
                    if (!seen.insert((*group)[i].id).second) {
                        fail(side + list + std::to_string(i) + "/id", "duplicates a player id used elsewhere in the match");
                    }
                }
            }
        }
    }
};

bool majorMinor(const std::string& version, int& major, int& minor)
{
    int patch = 0;
    char tail = 0;
    return std::sscanf(version.c_str(), "%d.%d.%d%c", &major, &minor, &patch, &tail) == 3;
}

}

std::vector<Problem> checkVersions(const json& doc)
{
    std::vector<Problem> problems;
    if (!doc.is_object()) {
        return problems;
    }

    const auto schema = doc.find("schemaVersion");
    if (schema != doc.end() && isInteger(*schema) && !integerIn(*schema, kSchemaVersion, kSchemaVersion)) {
        problems.push_back({"/schemaVersion", "schema version " + schema->dump() + " is not supported, this runner reads " + std::to_string(kSchemaVersion)});
    }

    const auto engine = doc.find("engineVersion");
    int major = 0;
    int minor = 0;
    int ourMajor = 0;
    int ourMinor = 0;
    majorMinor(kEngineVersion, ourMajor, ourMinor);
    if (engine != doc.end() && engine->is_string() && majorMinor(engine->get<std::string>(), major, minor)
        && (major != ourMajor || minor != ourMinor)) {
        problems.push_back({"/engineVersion", "engine version " + engine->get<std::string>() + " was expected, this runner is " + kEngineVersion});
    }
    return problems;
}

std::vector<Problem> parseMatchInput(const json& doc, MatchInput& input)
{
    Parser p;
    if (!doc.is_object()) {
        p.fail("", "must be a JSON object");
        return p.problems;
    }

    if (const json* v = p.field(doc, "", "schemaVersion", true); v != nullptr && !integerIn(*v, kSchemaVersion, kSchemaVersion)) {
        p.fail("/schemaVersion", "must be the integer " + std::to_string(kSchemaVersion));
    }

    if (const json* v = p.field(doc, "", "engineVersion", true)) {
        int major = 0;
        int minor = 0;
        if (!v->is_string() || !majorMinor(v->get<std::string>(), major, minor)) {
            p.fail("/engineVersion", "must be a version string like 0.1.0");
        } else {
            input.engineVersion = v->get<std::string>();
        }
    }

    if (const json* v = p.field(doc, "", "seed", true)) {
        if (!integerIn(*v, 0, static_cast<std::int64_t>(kMaxSeed))) {
            p.fail("/seed", "must be an integer from 0 to " + std::to_string(kMaxSeed));
        } else {
            input.seed = v->get<std::uint64_t>();
        }
    }

    if (const json* v = p.field(doc, "", "matchId", false)) {
        if (!v->is_string() || v->get<std::string>().size() > 128) {
            p.fail("/matchId", "must be a string of at most 128 bytes");
        } else {
            input.matchId = v->get<std::string>();
            input.hasMatchId = true;
        }
    }

    if (const json* ctx = p.field(doc, "", "context", false)) {
        if (!ctx->is_object()) {
            p.fail("/context", "must be an object");
        } else if (const json* adv = p.field(*ctx, "/context", "homeAdvantage", false)) {
            if (!adv->is_number() || adv->get<double>() < 0.5 || adv->get<double>() > 1.5) {
                p.fail("/context/homeAdvantage", "must be a number from 0.5 to 1.5");
            } else {
                input.homeAdvantage = adv->get<double>();
            }
        }
        if (const json* weather = p.field(*ctx, "/context", "weather", false)) {
            const auto kind = weather->is_object() ? weather->find("kind") : weather->end();
            if (!weather->is_object() || (kind != weather->end() && (!kind->is_string() || kind->get<std::string>() != "clear"))) {
                p.fail("/context/weather", "must be an object whose kind, when given, is clear");
            }
        }
    }

    for (const char* side : {"home", "away"}) {
        if (const json* v = p.field(doc, "", side, true)) {
            p.team(*v, std::string("/") + side, std::string_view(side) == "home" ? input.home : input.away);
        }
    }
    if (p.problems.empty()) {
        p.uniqueIds(input);
    }
    return p.problems;
}

}
