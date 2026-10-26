#pragma once

#include "nutmeg/debugview/render_state.h"
#include "nutmeg/debugview/view_transform.h"

struct SDL_Window;
struct SDL_Renderer;

namespace nutmeg::debugview {

class DebugRenderer {
public:
    DebugRenderer() = default;
    ~DebugRenderer();
    DebugRenderer(const DebugRenderer&) = delete;
    DebugRenderer& operator=(const DebugRenderer&) = delete;

    bool init(SDL_Window* window);
    void draw(const RenderState& state, const Overlays& overlays);

private:
    void drawPitch(const ViewTransform& view) const;
    void drawGrid(const ViewTransform& view) const;
    void drawPlayer(const ViewTransform& view, const PlayerView& player, bool home, const Overlays& overlays) const;
    void drawBall(const ViewTransform& view, const BallView& ball) const;
    void drawText(float x, float y, float scale, const char* text) const;

    SDL_Renderer* renderer_ = nullptr;
};

}
