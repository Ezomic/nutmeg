#include "nutmeg/core/binary.h"
#include "nutmeg/schema/player.h"

#include <catch2/catch_test_macros.hpp>

using namespace nutmeg;
using namespace nutmeg::schema;

namespace {

Partial<Attributes> legacySix()
{
    Partial<Attributes> partial;
    REQUIRE(partial.set("vision", 72));
    REQUIRE(partial.set("passing", 64));
    REQUIRE(partial.set("dribbling", 81));
    REQUIRE(partial.set("finishing", 55));
    REQUIRE(partial.set("tackling", 40));
    REQUIRE(partial.set("pace", 90));
    return partial;
}

}

TEST_CASE("a record with only the six legacy fields fills the rest with defaults")
{
    const Attributes a = withDefaults(legacySix());

    REQUIRE(a.vision == 72);
    REQUIRE(a.passing == 64);
    REQUIRE(a.dribbling == 81);
    REQUIRE(a.finishing == 55);
    REQUIRE(a.tackling == 40);
    REQUIRE(a.pace == 90);
    for (const int extra : {a.strength, a.stamina, a.aggression, a.crossing, a.firstTouch, a.composure, a.jumping}) {
        REQUIRE(extra == defaultRating);
    }
}

TEST_CASE("an empty record is valid and entirely default")
{
    REQUIRE(withDefaults(Partial<Attributes>{}) == Attributes{});
    REQUIRE(withDefaults(Partial<GoalkeeperAttributes>{}) == GoalkeeperAttributes{});
}

TEST_CASE("goalkeeper attributes accept just the legacy handling field")
{
    Partial<GoalkeeperAttributes> partial;
    REQUIRE(partial.set("handling", 77));

    const GoalkeeperAttributes g = withDefaults(partial);
    REQUIRE(g.handling == 77);
    REQUIRE(g.reflexes == defaultRating);
    REQUIRE(g.positioning == defaultRating);
    REQUIRE(g.diving == defaultRating);
    REQUIRE(g.kicking == defaultRating);
}

TEST_CASE("extras given in a record override their defaults")
{
    auto partial = legacySix();
    REQUIRE(partial.set("first_touch", 88));
    REQUIRE(partial.set("stamina", 33));

    const Attributes a = withDefaults(partial);
    REQUIRE(a.firstTouch == 88);
    REQUIRE(a.stamina == 33);
    REQUIRE(a.strength == defaultRating);
}

TEST_CASE("unknown attribute names are refused")
{
    Partial<Attributes> partial;
    REQUIRE_FALSE(partial.set("charisma", 50));
    REQUIRE_FALSE(partial.set("reflexes", 50));
}

TEST_CASE("withDefaults clamps to 1..100")
{
    Partial<Attributes> partial;
    REQUIRE(partial.set("vision", 0));
    REQUIRE(partial.set("passing", -40));
    REQUIRE(partial.set("pace", 250));
    REQUIRE(partial.set("jumping", 101));

    const Attributes a = withDefaults(partial);
    REQUIRE(a.vision == 1);
    REQUIRE(a.passing == 1);
    REQUIRE(a.pace == 100);
    REQUIRE(a.jumping == 100);
}

TEST_CASE("clamped brings out-of-range members back")
{
    Attributes a;
    a.tackling = 500;
    a.composure = -3;
    const Attributes c = a.clamped();
    REQUIRE(c.tackling == 100);
    REQUIRE(c.composure == 1);
    REQUIRE(c.vision == defaultRating);
}

TEST_CASE("scaled rounds half away from zero and clamps, like Pitch")
{
    Attributes a;
    a.vision = 75;
    a.passing = 41;
    a.dribbling = 40;
    a.finishing = 1;
    a.tackling = 100;
    a.pace = 3;

    const Attributes half = a.scaled(0.5);
    REQUIRE(half.vision == 38);
    REQUIRE(half.passing == 21);
    REQUIRE(half.dribbling == 20);
    REQUIRE(half.finishing == 1);
    REQUIRE(half.tackling == 50);
    REQUIRE(half.pace == 2);

    const Attributes up = a.scaled(1.3);
    REQUIRE(up.vision == 98);
    REQUIRE(up.tackling == 100);
    REQUIRE(up.dribbling == 52);

    REQUIRE(a.scaled(1.0) == a);
    REQUIRE(a.scaled(0.0).vision == 1);
}

TEST_CASE("scaled also moves the extras and the goalkeeper set")
{
    Attributes a;
    a.stamina = 80;
    REQUIRE(a.scaled(0.5).stamina == 40);
    REQUIRE(a.scaled(0.5).strength == 25);

    GoalkeeperAttributes g;
    g.reflexes = 90;
    REQUIRE(g.scaled(0.5).reflexes == 45);
}

TEST_CASE("plus nudges one attribute and clamps")
{
    Attributes a;
    a.pace = 98;

    const auto faster = a.plus("pace", 5);
    REQUIRE(faster.has_value());
    REQUIRE(faster->pace == 100);
    REQUIRE(faster->vision == a.vision);

    REQUIRE(a.plus("pace", -200)->pace == 1);
    REQUIRE(a.plus("first_touch", 10)->firstTouch == 60);
    REQUIRE_FALSE(a.plus("charisma", 1).has_value());

    GoalkeeperAttributes g;
    REQUIRE(g.plus("diving", 7)->diving == 57);
}

TEST_CASE("overall is the rounded mean of the six Pitch attributes only")
{
    Attributes a;
    a.vision = 60;
    a.passing = 70;
    a.dribbling = 80;
    a.finishing = 50;
    a.tackling = 40;
    a.pace = 65;
    REQUIRE(a.overall() == 61);

    a.vision = 58;
    REQUIRE(a.overall() == 61);

    a.strength = 100;
    REQUIRE(a.overall() == 61);
}

TEST_CASE("position and foot codes round-trip and reject unknowns")
{
    for (const auto p : {Position::Goalkeeper, Position::Defender, Position::Midfielder, Position::Forward}) {
        REQUIRE(positionFromCode(positionCode(p)) == p);
    }
    REQUIRE(positionCode(Position::Goalkeeper) == "GK");
    REQUIRE_FALSE(positionFromCode("ST").has_value());
    REQUIRE(footFromName("left") == Foot::Left);
    REQUIRE_FALSE(footFromName("Left").has_value());
}

TEST_CASE("a profile with only the legacy fields normalises to a valid profile")
{
    PlayerProfile profile;
    profile.id = 7;
    profile.name = "Nobody";
    profile.position = Position::Forward;
    profile.attributes = withDefaults(legacySix());

    REQUIRE(profile.foot == Foot::Right);
    REQUIRE(profile.heightCm == defaultHeightCm);
    REQUIRE(profile.weightKg == defaultWeightKg);
    REQUIRE(profile.normalised() == profile);
}

TEST_CASE("normalised clamps ratings, height and weight")
{
    PlayerProfile profile;
    profile.attributes.pace = 400;
    profile.goalkeeper.reflexes = 0;
    profile.heightCm = 300;
    profile.weightKg = 10;

    const PlayerProfile n = profile.normalised();
    REQUIRE(n.attributes.pace == 100);
    REQUIRE(n.goalkeeper.reflexes == 1);
    REQUIRE(n.heightCm == maxHeightCm);
    REQUIRE(n.weightKg == minWeightKg);
}

TEST_CASE("a player profile round-trips through the binary encoding")
{
    PlayerProfile profile;
    profile.id = 4242;
    profile.name = "Ada Keeper";
    profile.position = Position::Goalkeeper;
    profile.attributes = withDefaults(legacySix());
    profile.attributes.composure = 91;
    profile.goalkeeper = {80, 81, 82, 83, 84};
    profile.foot = Foot::Both;
    profile.heightCm = 195;
    profile.weightKg = 88;

    BinaryWriter w;
    w.write(profile);
    const auto bytes = w.bytes();

    BinaryReader r(bytes);
    PlayerProfile back;
    REQUIRE(r.read(back));
    REQUIRE(r.atEnd());
    REQUIRE(back == profile);
}

TEST_CASE("a truncated profile is refused")
{
    PlayerProfile profile;
    profile.name = "Cut Short";

    BinaryWriter w;
    w.write(profile);
    auto bytes = w.take();
    bytes.pop_back();

    BinaryReader r(bytes);
    PlayerProfile back;
    REQUIRE_FALSE(r.read(back));
}
