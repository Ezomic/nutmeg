#include "nutmeg/runner/match_input.h"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace nutmeg::runner;
using json = nlohmann::json;
namespace fs = std::filesystem;

namespace {

json validMatch()
{
    std::ifstream in(fs::path(NUTMEG_RUNNER_DIR) / "tests" / "data" / "valid_match.json", std::ios::binary);
    REQUIRE(in.good());
    return json::parse(std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()));
}

std::vector<Problem> parse(const json& doc)
{
    MatchInput input;
    return parseMatchInput(doc, input);
}

bool rejects(const json& doc, const std::string& path)
{
    const auto problems = parse(doc);
    return std::any_of(problems.begin(), problems.end(), [&](const Problem& p) { return p.path == path; });
}

}

TEST_CASE("the valid fixture parses into the match input")
{
    MatchInput input;
    REQUIRE(parseMatchInput(validMatch(), input).empty());

    REQUIRE(input.engineVersion == "0.1.0");
    REQUIRE(input.home.lineup.size() == 11);
    REQUIRE(input.home.lineup[0].position == "GK");
    REQUIRE(input.home.lineup[0].attributes.at("pace") > 0);
    REQUIRE(input.homeAdvantage > 1.0);
}

TEST_CASE("a document that is not an object is one problem at the root")
{
    const auto problems = parse(json::array());
    REQUIRE(problems.size() == 1);
    REQUIRE(problems[0].path.empty());
    REQUIRE(checkVersions(json::array()).empty());
}

TEST_CASE("every required top-level field is reported when missing")
{
    const auto problems = parse(json::object());
    std::vector<std::string> paths;
    for (const auto& p : problems) {
        paths.push_back(p.path);
    }
    REQUIRE(paths == std::vector<std::string>{"/schemaVersion", "/engineVersion", "/seed", "/home", "/away"});
}

TEST_CASE("the seed accepts the full safe-integer range and nothing beyond")
{
    json doc = validMatch();
    doc["seed"] = json::parse("0");
    REQUIRE(parse(doc).empty());
    doc["seed"] = 9007199254740991ull;
    REQUIRE(parse(doc).empty());
    doc["seed"] = 9007199254740992ull;
    REQUIRE(rejects(doc, "/seed"));
    doc["seed"] = 1.5;
    REQUIRE(rejects(doc, "/seed"));
    doc["seed"] = "1";
    REQUIRE(rejects(doc, "/seed"));
}

TEST_CASE("negative ids and shirts and attributes are rejected")
{
    json doc = validMatch();
    doc["home"]["lineup"][1]["id"] = -1;
    doc["home"]["lineup"][2]["shirt"] = -5;
    doc["home"]["lineup"][3]["attributes"]["pace"] = -3;

    REQUIRE(rejects(doc, "/home/lineup/1/id"));
    REQUIRE(rejects(doc, "/home/lineup/2/shirt"));
    REQUIRE(rejects(doc, "/home/lineup/3/attributes/pace"));
}

// Zero is built by parsing, as it arrives from Pitch: nlohmann reads a non-negative literal as
// number_unsigned, while assigning the C++ int 0 would give number_integer and hide the bug.
//
// Known bug: integerIn only checks the minimum for negative numbers, so an unsigned 0 passes
// where the schema says minimum 1.
TEST_CASE("an id or shirt or attribute of zero is rejected", "[!shouldfail]")
{
    json doc = validMatch();
    doc["home"]["lineup"][1]["id"] = json::parse("0");
    doc["home"]["lineup"][2]["shirt"] = json::parse("0");
    doc["home"]["lineup"][3]["attributes"]["pace"] = json::parse("0");

    CHECK(rejects(doc, "/home/lineup/1/id"));
    CHECK(rejects(doc, "/home/lineup/2/shirt"));
    CHECK(rejects(doc, "/home/lineup/3/attributes/pace"));
}

// Known bug: same integerIn minimum check, so schema version 0 is played instead of refused.
TEST_CASE("schema version 0 is not supported", "[!shouldfail]")
{
    json doc = validMatch();
    doc["schemaVersion"] = json::parse("0");

    CHECK_FALSE(checkVersions(doc).empty());
    CHECK(rejects(doc, "/schemaVersion"));
}

TEST_CASE("schema versions other than the current one are refused")
{
    json doc = validMatch();
    doc["schemaVersion"] = json::parse("2");
    REQUIRE(checkVersions(doc).size() == 1);
    REQUIRE(rejects(doc, "/schemaVersion"));

    doc["schemaVersion"] = -1;
    REQUIRE(checkVersions(doc).size() == 1);

    doc["schemaVersion"] = "1";
    REQUIRE(checkVersions(doc).empty());
    REQUIRE(rejects(doc, "/schemaVersion"));
}

TEST_CASE("engine versions match on major and minor only")
{
    json doc = validMatch();
    doc["engineVersion"] = "0.1.9";
    REQUIRE(checkVersions(doc).empty());
    REQUIRE(parse(doc).empty());

    doc["engineVersion"] = "0.2.0";
    REQUIRE(checkVersions(doc).size() == 1);

    for (const char* bad : {"0.1", "0.1.0-beta", "v0.1.0", ""}) {
        INFO(bad);
        doc["engineVersion"] = bad;
        REQUIRE(checkVersions(doc).empty());
        REQUIRE(rejects(doc, "/engineVersion"));
    }
}

TEST_CASE("shirt and slot ranges are enforced when given")
{
    json doc = validMatch();
    doc["home"]["lineup"][1]["shirt"] = json::parse("99");
    doc["home"]["lineup"][2]["slot"] = json::parse("10");
    REQUIRE(parse(doc).empty());

    doc["home"]["lineup"][1]["shirt"] = json::parse("100");
    doc["home"]["lineup"][2]["slot"] = json::parse("11");
    REQUIRE(rejects(doc, "/home/lineup/1/shirt"));
    REQUIRE(rejects(doc, "/home/lineup/2/slot"));
}

TEST_CASE("attributes must be integers from 1 to 100 including extra ones")
{
    json doc = validMatch();
    doc["home"]["lineup"][1]["attributes"]["pace"] = json::parse("100");
    doc["home"]["lineup"][2]["attributes"]["pace"] = json::parse("1");
    REQUIRE(parse(doc).empty());

    doc["home"]["lineup"][1]["attributes"]["stamina"] = json::parse("101");
    doc["home"]["lineup"][2]["attributes"]["pace"] = 50.5;
    doc["home"]["lineup"][3]["attributes"] = json::array();
    REQUIRE(rejects(doc, "/home/lineup/1/attributes/stamina"));
    REQUIRE(rejects(doc, "/home/lineup/2/attributes/pace"));
    REQUIRE(rejects(doc, "/home/lineup/3/attributes"));
}

TEST_CASE("names must be non-empty and at most 128 bytes")
{
    json doc = validMatch();
    doc["home"]["name"] = std::string(128, 'x');
    REQUIRE(parse(doc).empty());

    doc["home"]["name"] = std::string(129, 'x');
    doc["away"]["name"] = "";
    doc["home"]["lineup"][1]["name"] = 7;
    REQUIRE(rejects(doc, "/home/name"));
    REQUIRE(rejects(doc, "/away/name"));
    REQUIRE(rejects(doc, "/home/lineup/1/name"));
}

TEST_CASE("positions and formations and mentalities come from fixed lists")
{
    json doc = validMatch();
    doc["home"]["lineup"][1]["position"] = "ST";
    doc["home"]["formation"] = "4-4-2";
    doc["away"]["mentality"] = "Attacking";

    REQUIRE(rejects(doc, "/home/lineup/1/position"));
    REQUIRE(rejects(doc, "/home/formation"));
    REQUIRE(rejects(doc, "/away/mentality"));
}

TEST_CASE("a lineup needs exactly one goalkeeper")
{
    json twoKeepers = validMatch();
    twoKeepers["home"]["lineup"][1]["position"] = "GK";
    REQUIRE(rejects(twoKeepers, "/home/lineup"));

    json noKeeper = validMatch();
    noKeeper["away"]["lineup"][0]["position"] = "DF";
    REQUIRE(rejects(noKeeper, "/away/lineup"));
}

TEST_CASE("the bench is optional and holds at most 12 players")
{
    json doc = validMatch();
    doc["home"].erase("bench");
    REQUIRE(parse(doc).empty());

    json full = validMatch();
    json& bench = full["home"]["bench"];
    const json sub = bench[0];
    for (int id = 900; bench.size() < 13; ++id) {
        bench.push_back(sub);
        bench.back()["id"] = id;
    }
    REQUIRE(rejects(full, "/home/bench"));
    bench.erase(bench.end() - 1);
    REQUIRE(parse(full).empty());
}

TEST_CASE("player ids are unique across both teams and the benches")
{
    json doc = validMatch();
    doc["away"]["bench"][0]["id"] = doc["home"]["lineup"][0]["id"];

    const auto problems = parse(doc);
    REQUIRE(problems.size() == 1);
    REQUIRE(problems[0].path == "/away/bench/0/id");
}

TEST_CASE("home advantage is bounded and matchId is optional")
{
    json doc = validMatch();
    for (const double ok : {0.5, 1.5}) {
        doc["context"]["homeAdvantage"] = ok;
        REQUIRE(parse(doc).empty());
    }
    for (const json bad : {json(0.49), json(1.51), json("1.0")}) {
        doc["context"]["homeAdvantage"] = bad;
        REQUIRE(rejects(doc, "/context/homeAdvantage"));
    }

    json bare = validMatch();
    bare.erase("matchId");
    bare.erase("context");
    MatchInput input;
    REQUIRE(parseMatchInput(bare, input).empty());
    REQUIRE_FALSE(input.hasMatchId);
    REQUIRE(input.homeAdvantage == 1.0);

    bare["matchId"] = std::string(129, 'm');
    REQUIRE(rejects(bare, "/matchId"));
}

TEST_CASE("only clear weather is accepted")
{
    json doc = validMatch();
    doc["context"]["weather"] = json::object();
    REQUIRE(parse(doc).empty());

    doc["context"]["weather"]["kind"] = "rain";
    REQUIRE(rejects(doc, "/context/weather"));
    doc["context"]["weather"] = "clear";
    REQUIRE(rejects(doc, "/context/weather"));
}
