#pragma once

#include "nutmeg/core/reflect.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string_view>
#include <tuple>
#include <utility>

namespace nutmeg::schema {

// Every rating lives on the same 1..100 scale as the Pitch simulation. The
// full field list, defaults and the system that reads each one is in
// docs/attributes.md.
inline constexpr int minRating = 1;
inline constexpr int maxRating = 100;
inline constexpr int defaultRating = 50;

constexpr int clampRating(int value)
{
    return value < minRating ? minRating : (value > maxRating ? maxRating : value);
}

// Outfield attributes. The first six are the Pitch attributes, unchanged in
// meaning and scale. The rest are optional extras for real-time play.
struct Attributes {
    int vision = defaultRating;
    int passing = defaultRating;
    int dribbling = defaultRating;
    int finishing = defaultRating;
    int tackling = defaultRating;
    int pace = defaultRating;

    int strength = defaultRating;
    int stamina = defaultRating;
    int aggression = defaultRating;
    int crossing = defaultRating;
    int firstTouch = defaultRating;
    int composure = defaultRating;
    int jumping = defaultRating;

    Attributes clamped() const;
    Attributes scaled(double factor) const;
    std::optional<Attributes> plus(std::string_view attribute, int delta) const;
    int overall() const;

    bool operator==(const Attributes&) const = default;
};

struct GoalkeeperAttributes {
    int reflexes = defaultRating;
    int handling = defaultRating;
    int positioning = defaultRating;
    int diving = defaultRating;
    int kicking = defaultRating;

    GoalkeeperAttributes clamped() const;
    GoalkeeperAttributes scaled(double factor) const;
    std::optional<GoalkeeperAttributes> plus(std::string_view attribute, int delta) const;

    bool operator==(const GoalkeeperAttributes&) const = default;
};

template <class T>
struct AttributeField {
    std::string_view name;
    int T::*member;
    int defaultValue;
};

// One row per field: the serialised (snake_case) name, the member and the
// default used when a record leaves the field out.
template <class T>
struct AttributeTable;

template <>
struct AttributeTable<Attributes> {
    static constexpr std::array fields = {
        AttributeField<Attributes>{"vision", &Attributes::vision, defaultRating},
        AttributeField<Attributes>{"passing", &Attributes::passing, defaultRating},
        AttributeField<Attributes>{"dribbling", &Attributes::dribbling, defaultRating},
        AttributeField<Attributes>{"finishing", &Attributes::finishing, defaultRating},
        AttributeField<Attributes>{"tackling", &Attributes::tackling, defaultRating},
        AttributeField<Attributes>{"pace", &Attributes::pace, defaultRating},
        AttributeField<Attributes>{"strength", &Attributes::strength, defaultRating},
        AttributeField<Attributes>{"stamina", &Attributes::stamina, defaultRating},
        AttributeField<Attributes>{"aggression", &Attributes::aggression, defaultRating},
        AttributeField<Attributes>{"crossing", &Attributes::crossing, defaultRating},
        AttributeField<Attributes>{"first_touch", &Attributes::firstTouch, defaultRating},
        AttributeField<Attributes>{"composure", &Attributes::composure, defaultRating},
        AttributeField<Attributes>{"jumping", &Attributes::jumping, defaultRating},
    };
};

template <>
struct AttributeTable<GoalkeeperAttributes> {
    static constexpr std::array fields = {
        AttributeField<GoalkeeperAttributes>{"reflexes", &GoalkeeperAttributes::reflexes, defaultRating},
        AttributeField<GoalkeeperAttributes>{"handling", &GoalkeeperAttributes::handling, defaultRating},
        AttributeField<GoalkeeperAttributes>{"positioning", &GoalkeeperAttributes::positioning, defaultRating},
        AttributeField<GoalkeeperAttributes>{"diving", &GoalkeeperAttributes::diving, defaultRating},
        AttributeField<GoalkeeperAttributes>{"kicking", &GoalkeeperAttributes::kicking, defaultRating},
    };
};

// What a parser has read from a record: each field present or absent, values
// as given (not yet clamped). withDefaults() turns it into a valid set.
template <class T>
struct Partial {
    std::array<std::optional<int>, AttributeTable<T>::fields.size()> values{};

    // False when the name is not an attribute of T, so a parser can reject it.
    bool set(std::string_view name, int value)
    {
        for (std::size_t i = 0; i < values.size(); ++i) {
            if (AttributeTable<T>::fields[i].name == name) {
                values[i] = value;
                return true;
            }
        }
        return false;
    }
};

template <class T>
T withDefaults(const Partial<T>& partial)
{
    T result;
    for (std::size_t i = 0; i < partial.values.size(); ++i) {
        const auto& f = AttributeTable<T>::fields[i];
        result.*(f.member) = clampRating(partial.values[i].value_or(f.defaultValue));
    }
    return result;
}

template <class T>
T clampAll(T set)
{
    for (const auto& f : AttributeTable<T>::fields) {
        set.*(f.member) = clampRating(set.*(f.member));
    }
    return set;
}

// Same rule as the Pitch simulation: multiply, round half away from zero,
// clamp to 1..100.
template <class T>
T scaleAll(T set, double factor)
{
    for (const auto& f : AttributeTable<T>::fields) {
        set.*(f.member) = clampRating(static_cast<int>(std::round(static_cast<double>(set.*(f.member)) * factor)));
    }
    return set;
}

template <class T>
std::optional<T> plusOne(T set, std::string_view attribute, int delta)
{
    for (const auto& f : AttributeTable<T>::fields) {
        if (f.name == attribute) {
            set.*(f.member) = clampRating(set.*(f.member) + delta);
            return set;
        }
    }
    return std::nullopt;
}

// Reflection derived from the table, so binary encoding order is table order.
template <class T>
struct TableReflect {
    static constexpr auto fields()
    {
        return build(std::make_index_sequence<AttributeTable<T>::fields.size()>{});
    }

private:
    template <std::size_t... I>
    static constexpr auto build(std::index_sequence<I...>)
    {
        return std::make_tuple(nutmeg::field(AttributeTable<T>::fields[I].name, AttributeTable<T>::fields[I].member)...);
    }
};

inline Attributes Attributes::clamped() const { return clampAll(*this); }
inline Attributes Attributes::scaled(double factor) const { return scaleAll(*this, factor); }
inline std::optional<Attributes> Attributes::plus(std::string_view attribute, int delta) const
{
    return plusOne(*this, attribute, delta);
}

// Rounded mean of the six Pitch attributes. The extras do not move it, so a
// Pitch overall rating means the same here.
inline int Attributes::overall() const
{
    const int sum = vision + passing + dribbling + finishing + tackling + pace;
    return static_cast<int>(std::round(static_cast<double>(sum) / 6.0));
}

inline GoalkeeperAttributes GoalkeeperAttributes::clamped() const { return clampAll(*this); }
inline GoalkeeperAttributes GoalkeeperAttributes::scaled(double factor) const { return scaleAll(*this, factor); }
inline std::optional<GoalkeeperAttributes> GoalkeeperAttributes::plus(std::string_view attribute, int delta) const
{
    return plusOne(*this, attribute, delta);
}

}

template <>
struct nutmeg::Reflect<nutmeg::schema::Attributes> : nutmeg::schema::TableReflect<nutmeg::schema::Attributes> {};

template <>
struct nutmeg::Reflect<nutmeg::schema::GoalkeeperAttributes>
    : nutmeg::schema::TableReflect<nutmeg::schema::GoalkeeperAttributes> {};
