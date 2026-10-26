#pragma once

#include <cmath>

namespace nutmeg::physics {

// Metres. x runs along the pitch, y across it, z is up.
struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;

    constexpr bool operator==(const Vec2&) const = default;
};

constexpr Vec2 operator+(Vec2 a, Vec2 b) { return {a.x + b.x, a.y + b.y}; }
constexpr Vec2 operator-(Vec2 a, Vec2 b) { return {a.x - b.x, a.y - b.y}; }
constexpr Vec2 operator*(Vec2 a, float s) { return {a.x * s, a.y * s}; }
constexpr Vec2 operator*(float s, Vec2 a) { return a * s; }
constexpr float dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
inline float length(Vec2 a) { return std::sqrt(dot(a, a)); }
inline float distance(Vec2 a, Vec2 b) { return length(a - b); }

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    constexpr bool operator==(const Vec3&) const = default;
};

constexpr Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
constexpr Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
constexpr Vec3 operator*(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
constexpr Vec3 operator*(float s, Vec3 a) { return a * s; }
constexpr float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
constexpr Vec3 cross(Vec3 a, Vec3 b)
{
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline float length(Vec3 a) { return std::sqrt(dot(a, a)); }
constexpr Vec2 xy(Vec3 a) { return {a.x, a.y}; }

}
