#pragma once

#include "nutmeg/debugview/render_state.h"

namespace nutmeg::debugview {

struct ScreenPoint {
    float x = 0.0f;
    float y = 0.0f;
};

// Fits the whole pitch, plus a margin, into a window while keeping its aspect
// ratio; the leftover space is split evenly either side.
class ViewTransform {
public:
    ViewTransform(int windowWidth, int windowHeight, float marginMetres = 4.0f);

    ScreenPoint toScreen(Vec2 world) const;
    Vec2 toWorld(ScreenPoint screen) const;
    float pixelsPerMetre() const { return scale_; }

private:
    float scale_ = 1.0f;
    float centreX_ = 0.0f;
    float centreY_ = 0.0f;
};

}
