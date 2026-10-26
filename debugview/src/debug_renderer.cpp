#include "nutmeg/debugview/debug_renderer.h"

#include "nutmeg/core/log.h"

#include <SDL3/SDL.h>

#include <cmath>
#include <numbers>
#include <string>
#include <vector>

namespace nutmeg::debugview {

namespace {

constexpr float kPi = std::numbers::pi_v<float>;
constexpr float kPlayerRadius = 1.2f;
constexpr int kCircleSegments = 40;

struct Colour {
    Uint8 r, g, b, a = 255;
};

constexpr Colour kGrass{34, 120, 52};
constexpr Colour kGrassStripe{40, 130, 58};
constexpr Colour kLine{235, 235, 235};
constexpr Colour kHome{220, 50, 50};
constexpr Colour kAway{50, 110, 230};
constexpr Colour kBallColour{250, 250, 250};
constexpr Colour kShadow{0, 0, 0, 90};
constexpr Colour kGridColour{255, 255, 255, 40};

void setColour(SDL_Renderer* renderer, Colour c)
{
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
}

void fillCircle(SDL_Renderer* renderer, ScreenPoint centre, float radius, Colour c)
{
    const SDL_FColor colour{c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f};
    std::vector<SDL_Vertex> vertices;
    vertices.reserve(kCircleSegments + 1);
    vertices.push_back({{centre.x, centre.y}, colour, {}});
    std::vector<int> indices;
    indices.reserve(kCircleSegments * 3);
    for (int i = 0; i < kCircleSegments; ++i) {
        const float angle = 2.0f * kPi * static_cast<float>(i) / kCircleSegments;
        vertices.push_back({{centre.x + radius * std::cos(angle), centre.y + radius * std::sin(angle)}, colour, {}});
        indices.push_back(0);
        indices.push_back(1 + i);
        indices.push_back(1 + (i + 1) % kCircleSegments);
    }
    SDL_RenderGeometry(renderer, nullptr, vertices.data(), static_cast<int>(vertices.size()), indices.data(), static_cast<int>(indices.size()));
}

void strokeArc(SDL_Renderer* renderer, const ViewTransform& view, Vec2 centre, float radius, float from, float to)
{
    std::vector<SDL_FPoint> points;
    constexpr int kSteps = 48;
    for (int i = 0; i <= kSteps; ++i) {
        const float angle = from + (to - from) * static_cast<float>(i) / kSteps;
        const ScreenPoint p = view.toScreen({centre.x + radius * std::cos(angle), centre.y + radius * std::sin(angle)});
        points.push_back({p.x, p.y});
    }
    SDL_RenderLines(renderer, points.data(), static_cast<int>(points.size()));
}

void strokeRect(SDL_Renderer* renderer, const ViewTransform& view, float left, float bottom, float right, float top)
{
    const ScreenPoint a = view.toScreen({left, top});
    const ScreenPoint b = view.toScreen({right, bottom});
    const SDL_FRect rect{a.x, a.y, b.x - a.x, b.y - a.y};
    SDL_RenderRect(renderer, &rect);
}

}

DebugRenderer::~DebugRenderer()
{
    if (renderer_ != nullptr) {
        SDL_DestroyRenderer(renderer_);
    }
}

bool DebugRenderer::init(SDL_Window* window)
{
    renderer_ = SDL_CreateRenderer(window, nullptr);
    if (renderer_ == nullptr) {
        logf(LogLevel::Error, "SDL_CreateRenderer failed: {}", SDL_GetError());
        return false;
    }
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderVSync(renderer_, 1);
    return true;
}

void DebugRenderer::draw(const RenderState& state, const Overlays& overlays)
{
    int width = 0;
    int height = 0;
    SDL_GetCurrentRenderOutputSize(renderer_, &width, &height);
    const ViewTransform view(width, height);

    setColour(renderer_, {20, 70, 32});
    SDL_RenderClear(renderer_);
    drawPitch(view);
    if (overlays.grid) {
        drawGrid(view);
    }
    drawBall(view, state.ball);
    for (const PlayerView& player : state.away) {
        drawPlayer(view, player, false, overlays);
    }
    for (const PlayerView& player : state.home) {
        drawPlayer(view, player, true, overlays);
    }
    if (overlays.text) {
        setColour(renderer_, {255, 255, 255});
        drawText(10.0f, 10.0f, 2.0f, state.overlayText.c_str());
        drawText(10.0f, static_cast<float>(height) - 22.0f, 1.5f, "N numbers  F facing  G grid  T text  Esc quit");
    }
    SDL_RenderPresent(renderer_);
}

void DebugRenderer::drawPitch(const ViewTransform& view) const
{
    constexpr float halfL = kPitchLength / 2.0f;
    constexpr float halfW = kPitchWidth / 2.0f;
    constexpr int kStripes = 10;
    for (int i = 0; i < kStripes; i += 2) {
        const float left = -halfL + kPitchLength * static_cast<float>(i) / kStripes;
        const ScreenPoint a = view.toScreen({left, halfW});
        const ScreenPoint b = view.toScreen({left + kPitchLength / kStripes, -halfW});
        setColour(renderer_, kGrassStripe);
        const SDL_FRect rect{a.x, a.y, b.x - a.x, b.y - a.y};
        SDL_RenderFillRect(renderer_, &rect);
    }

    setColour(renderer_, kLine);
    strokeRect(renderer_, view, -halfL, -halfW, halfL, halfW);
    const ScreenPoint top = view.toScreen({0.0f, halfW});
    const ScreenPoint bottom = view.toScreen({0.0f, -halfW});
    SDL_RenderLine(renderer_, top.x, top.y, bottom.x, bottom.y);
    strokeArc(renderer_, view, {0.0f, 0.0f}, 9.15f, 0.0f, 2.0f * kPi);
    fillCircle(renderer_, view.toScreen({0.0f, 0.0f}), 0.25f * view.pixelsPerMetre(), kLine);

    for (const float side : {-1.0f, 1.0f}) {
        const float goalLine = side * halfL;
        const float inward = -side;
        strokeRect(renderer_, view, std::min(goalLine, goalLine + inward * 16.5f), -20.16f, std::max(goalLine, goalLine + inward * 16.5f), 20.16f);
        strokeRect(renderer_, view, std::min(goalLine, goalLine + inward * 5.5f), -9.16f, std::max(goalLine, goalLine + inward * 5.5f), 9.16f);
        const Vec2 spot{goalLine + inward * 11.0f, 0.0f};
        fillCircle(renderer_, view.toScreen(spot), 0.25f * view.pixelsPerMetre(), kLine);
        // The D: the part of the 9.15 m circle around the spot that lies outside the box.
        const float reach = std::acos(5.5f / 9.15f);
        const float centreAngle = side < 0.0f ? 0.0f : kPi;
        strokeArc(renderer_, view, spot, 9.15f, centreAngle - reach, centreAngle + reach);

        setColour(renderer_, {255, 255, 255});
        strokeRect(renderer_, view, std::min(goalLine, goalLine + side * 2.0f), -3.66f, std::max(goalLine, goalLine + side * 2.0f), 3.66f);
        setColour(renderer_, kLine);
    }
}

void DebugRenderer::drawGrid(const ViewTransform& view) const
{
    setColour(renderer_, kGridColour);
    for (int x = -50; x <= 50; x += 10) {
        const ScreenPoint a = view.toScreen({static_cast<float>(x), kPitchWidth / 2.0f});
        const ScreenPoint b = view.toScreen({static_cast<float>(x), -kPitchWidth / 2.0f});
        SDL_RenderLine(renderer_, a.x, a.y, b.x, b.y);
    }
    for (int y = -30; y <= 30; y += 10) {
        const ScreenPoint a = view.toScreen({-kPitchLength / 2.0f, static_cast<float>(y)});
        const ScreenPoint b = view.toScreen({kPitchLength / 2.0f, static_cast<float>(y)});
        SDL_RenderLine(renderer_, a.x, a.y, b.x, b.y);
    }
}

void DebugRenderer::drawPlayer(const ViewTransform& view, const PlayerView& player, bool home, const Overlays& overlays) const
{
    const ScreenPoint centre = view.toScreen(player.position);
    const float radius = kPlayerRadius * view.pixelsPerMetre();
    if (player.controlled) {
        fillCircle(renderer_, centre, radius * 1.45f, {255, 230, 60, 255});
    }
    fillCircle(renderer_, centre, radius, {20, 20, 20});
    fillCircle(renderer_, centre, radius * 0.88f, home ? kHome : kAway);

    if (overlays.facing) {
        const float dx = std::cos(player.facing);
        const float dy = -std::sin(player.facing);
        const float tipLength = radius * 1.9f;
        const SDL_FPoint tip{centre.x + dx * tipLength, centre.y + dy * tipLength};
        const SDL_FPoint base{centre.x + dx * radius * 0.9f, centre.y + dy * radius * 0.9f};
        const float wing = radius * 0.45f;
        setColour(renderer_, {255, 255, 255});
        SDL_RenderLine(renderer_, base.x, base.y, tip.x, tip.y);
        SDL_RenderLine(renderer_, tip.x, tip.y, tip.x - dx * wing - dy * wing, tip.y - dy * wing + dx * wing);
        SDL_RenderLine(renderer_, tip.x, tip.y, tip.x - dx * wing + dy * wing, tip.y - dy * wing - dx * wing);
    }
    if (overlays.numbers) {
        const std::string label = std::to_string(player.number);
        const float scale = radius * 0.9f / 8.0f;
        const float width = static_cast<float>(label.size()) * 8.0f * scale;
        setColour(renderer_, {255, 255, 255});
        drawText(centre.x - width * 0.5f, centre.y - 4.0f * scale, scale, label.c_str());
    }
}

void DebugRenderer::drawBall(const ViewTransform& view, const BallView& ball) const
{
    const ScreenPoint ground = view.toScreen(ball.position);
    const float metre = view.pixelsPerMetre();
    fillCircle(renderer_, ground, 0.5f * metre, kShadow);
    const ScreenPoint lifted{ground.x, ground.y - ball.height * metre * 0.6f};
    const float radius = (0.5f + 0.15f * ball.height) * metre;
    fillCircle(renderer_, lifted, radius * 1.15f, {20, 20, 20});
    fillCircle(renderer_, lifted, radius, kBallColour);
}

void DebugRenderer::drawText(float x, float y, float scale, const char* text) const
{
    SDL_SetRenderScale(renderer_, scale, scale);
    SDL_RenderDebugText(renderer_, x / scale, y / scale, text);
    SDL_SetRenderScale(renderer_, 1.0f, 1.0f);
}

}
