#include "nutmeg/runner/cli.h"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

using namespace nutmeg::runner;
using json = nlohmann::json;
namespace fs = std::filesystem;

namespace {

const fs::path kRunner = NUTMEG_RUNNER_DIR;

std::string slurp(const fs::path& path)
{
    std::ifstream in(path, std::ios::binary);
    REQUIRE(in.good());
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

struct Outcome {
    int code = -1;
    std::string out;
    std::string err;
};

Outcome run(const std::vector<std::string>& args)
{
    std::ostringstream out;
    std::ostringstream err;
    const int code = runCli(args, out, err);
    return {code, out.str(), err.str()};
}

std::string data(const char* name) { return (kRunner / "tests" / "data" / name).string(); }

json schema(const char* name) { return json::parse(slurp(kRunner / "schema" / name)); }

const json& resolve(const json& root, const json& node)
{
    if (!node.contains("$ref")) {
        return node;
    }
    const json* target = &root;
    std::istringstream path(node["$ref"].get<std::string>().substr(2));
    for (std::string part; std::getline(path, part, '/');) {
        target = &target->at(part);
    }
    return *target;
}

// Covers the keywords the schemas use: type, enum, minimum, maximum, required, properties,
// additionalProperties, items. Enough to keep the files and the runner from drifting apart.
void conforms(const json& root, const json& rawSchema, const json& value, const std::string& where)
{
    const json& s = resolve(root, rawSchema);
    INFO(where);
    if (s.contains("type")) {
        const std::string type = s["type"];
        if (type == "integer") {
            REQUIRE(value.is_number_integer());
        } else if (type == "number") {
            REQUIRE(value.is_number());
        } else if (type == "string") {
            REQUIRE(value.is_string());
        } else if (type == "boolean") {
            REQUIRE(value.is_boolean());
        } else if (type == "object") {
            REQUIRE(value.is_object());
        } else if (type == "array") {
            REQUIRE(value.is_array());
        }
    }
    if (s.contains("enum")) {
        REQUIRE(std::find(s["enum"].begin(), s["enum"].end(), value) != s["enum"].end());
    }
    if (s.contains("minimum")) {
        REQUIRE(value.get<double>() >= s["minimum"].get<double>());
    }
    if (s.contains("maximum")) {
        REQUIRE(value.get<double>() <= s["maximum"].get<double>());
    }
    if (value.is_object()) {
        for (const auto& key : s.value("required", json::array())) {
            REQUIRE(value.contains(key.get<std::string>()));
        }
        const json empty = json::object();
        const json& props = s.contains("properties") ? s["properties"] : empty;
        for (const auto& [key, child] : value.items()) {
            if (props.contains(key)) {
                conforms(root, props[key], child, where + "/" + key);
            } else {
                REQUIRE(s.contains("additionalProperties"));
                conforms(root, s["additionalProperties"], child, where + "/" + key);
            }
        }
    }
    if (value.is_array()) {
        if (s.contains("minItems")) {
            REQUIRE(value.size() >= s["minItems"].get<std::size_t>());
        }
        if (s.contains("maxItems")) {
            REQUIRE(value.size() <= s["maxItems"].get<std::size_t>());
        }
        for (std::size_t i = 0; i < value.size() && s.contains("items"); ++i) {
            conforms(root, s["items"], value[i], where + "/" + std::to_string(i));
        }
    }
}

}

TEST_CASE("a valid match produces the golden result")
{
    const auto outcome = run({data("valid_match.json")});

    REQUIRE(outcome.code == kExitOk);
    REQUIRE(outcome.err.empty());
    REQUIRE(outcome.out == slurp(kRunner / "tests" / "data" / "expected_result.json"));
}

TEST_CASE("the result file is byte-identical for the same input")
{
    const fs::path file = fs::temp_directory_path() / "nutmeg_runner_test_result.json";
    fs::remove(file);

    REQUIRE(run({data("valid_match.json"), "-o", file.string()}).code == kExitOk);
    REQUIRE(slurp(file) == slurp(kRunner / "tests" / "data" / "expected_result.json"));
    fs::remove(file);
}

TEST_CASE("a different seed gives a different result")
{
    json doc = json::parse(slurp(data("valid_match.json")));
    doc["seed"] = 987654321;
    const fs::path file = fs::temp_directory_path() / "nutmeg_runner_test_seed.json";
    std::ofstream(file) << doc.dump();

    const auto outcome = run({file.string()});
    fs::remove(file);

    REQUIRE(outcome.code == kExitOk);
    REQUIRE(outcome.out != slurp(kRunner / "tests" / "data" / "expected_result.json"));
}

TEST_CASE("the valid input and its result conform to the published schemas")
{
    const json inputSchema = schema("match-input.schema.json");
    const json resultSchema = schema("match-result.schema.json");

    conforms(inputSchema, inputSchema, json::parse(slurp(data("valid_match.json"))), "");
    conforms(resultSchema, resultSchema, json::parse(run({data("valid_match.json")}).out), "");
}

TEST_CASE("result statistics agree with the event list")
{
    const json result = json::parse(run({data("valid_match.json")}).out);

    for (const char* side : {"home", "away"}) {
        int goals = 0;
        int fouls = 0;
        int corners = 0;
        int yellow = 0;
        for (const auto& event : result["events"]) {
            if (event["team"] != side) {
                continue;
            }
            goals += event["type"] == "goal";
            fouls += event["type"] == "foul";
            corners += event["type"] == "corner";
            yellow += event["type"] == "yellow_card";
        }
        const auto& stats = result["teams"][side];
        REQUIRE(stats["goals"] == goals);
        REQUIRE(stats["fouls"] == fouls);
        REQUIRE(stats["corners"] == corners);
        REQUIRE(stats["yellowCards"] == yellow);
        REQUIRE(result["score"][side] == goals);
    }
    REQUIRE(result["teams"]["home"]["possession"].get<double>() + result["teams"]["away"]["possession"].get<double>() == 100.0);
}

TEST_CASE("an invalid match exits 2 and names every problem")
{
    const auto outcome = run({data("invalid_match.json")});

    REQUIRE(outcome.code == kExitInvalidInput);
    REQUIRE(outcome.out.empty());
    REQUIRE(outcome.err == slurp(kRunner / "tests" / "data" / "expected_invalid.txt"));
}

TEST_CASE("a file that is not JSON exits 2")
{
    const fs::path file = fs::temp_directory_path() / "nutmeg_runner_test_garbage.json";
    std::ofstream(file) << "{not json";

    const auto outcome = run({file.string()});
    fs::remove(file);

    REQUIRE(outcome.code == kExitInvalidInput);
}

TEST_CASE("an unexpected engine version exits 3")
{
    const auto outcome = run({data("version_mismatch.json")});

    REQUIRE(outcome.code == kExitUnsupportedVersion);
    REQUIRE(outcome.out.empty());
    REQUIRE(outcome.err == slurp(kRunner / "tests" / "data" / "expected_version_mismatch.txt"));
}

TEST_CASE("an unknown schema version exits 3")
{
    REQUIRE(run({data("schema_version_mismatch.json")}).code == kExitUnsupportedVersion);
}

TEST_CASE("usage errors exit 1")
{
    REQUIRE(run({}).code == kExitUsage);
    REQUIRE(run({"--nope"}).code == kExitUsage);
    REQUIRE(run({data("valid_match.json"), "-o"}).code == kExitUsage);
    REQUIRE(run({(kRunner / "tests" / "data" / "missing.json").string()}).code == kExitUsage);
}

TEST_CASE("a rejected match writes no output file")
{
    const fs::path file = fs::temp_directory_path() / "nutmeg_runner_test_rejected.json";
    fs::remove(file);

    REQUIRE(run({data("invalid_match.json"), "-o", file.string()}).code == kExitInvalidInput);
    REQUIRE_FALSE(fs::exists(file));
}
