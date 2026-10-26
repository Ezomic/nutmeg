#include "nutmeg/debugview/view_transform.h"

#include <algorithm>

namespace nutmeg::debugview {

ViewTransform::ViewTransform(int windowWidth, int windowHeight, float marginMetres)
{
    const float width = static_cast<float>(std::max(windowWidth, 1));
    const float height = static_cast<float>(std::max(windowHeight, 1));
    scale_ = std::min(width / (kPitchLength + 2.0f * marginMetres), height / (kPitchWidth + 2.0f * marginMetres));
    centreX_ = width * 0.5f;
    centreY_ = height * 0.5f;
}

ScreenPoint ViewTransform::toScreen(Vec2 world) const
{
    return {centreX_ + world.x * scale_, centreY_ - world.y * scale_};
}

Vec2 ViewTransform::toWorld(ScreenPoint screen) const
{
    return {(screen.x - centreX_) / scale_, (centreY_ - screen.y) / scale_};
}

}
