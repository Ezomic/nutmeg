#include "nutmeg/core/binary.h"
#include "nutmeg/core/input.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

using namespace nutmeg;

namespace {

struct Nested {
    float x = 0.0f;
    double y = 0.0;
    std::string label;
    std::vector<std::uint32_t> numbers;
    InputFrame input;
};

}

template <>
struct nutmeg::Reflect<Nested> {
    static constexpr auto fields()
    {
        return std::make_tuple(field("x", &Nested::x), field("y", &Nested::y), field("label", &Nested::label),
            field("numbers", &Nested::numbers), field("input", &Nested::input));
    }
};

TEST_CASE("reflected structs round-trip through the binary encoding")
{
    Nested value;
    value.x = 1.5f;
    value.y = -2.25e6;
    value.label = "striker";
    value.numbers = {1, 2, 3, 99};
    value.input = {InputFrame::Sprint, 0.5f, -1.0f};

    BinaryWriter w;
    w.write(value);
    const auto bytes = w.bytes();

    BinaryReader r(bytes);
    Nested back;
    REQUIRE(r.read(back));
    REQUIRE(r.atEnd());
    REQUIRE(back.x == value.x);
    REQUIRE(back.y == value.y);
    REQUIRE(back.label == value.label);
    REQUIRE(back.numbers == value.numbers);
    REQUIRE(back.input == value.input);
}

TEST_CASE("truncated input is refused, never read past the end")
{
    Nested value;
    value.label = "a long enough label";
    value.numbers.assign(50, 7);
    BinaryWriter w;
    w.write(value);
    const auto bytes = w.bytes();

    for (std::size_t cut : {std::size_t{0}, std::size_t{10}, bytes.size() / 2, bytes.size() - 1}) {
        BinaryReader r(std::span(bytes.data(), cut));
        Nested back;
        REQUIRE_FALSE(r.read(back));
    }
}

TEST_CASE("a corrupt length cannot allocate more than the file holds")
{
    std::vector<std::uint8_t> bytes = {0xff, 0xff, 0xff, 0xff, 'x'};
    BinaryReader r(bytes);
    std::string s;
    REQUIRE_FALSE(r.read(s));
    BinaryReader r2(bytes);
    std::vector<std::uint32_t> v;
    REQUIRE_FALSE(r2.read(v));
}
