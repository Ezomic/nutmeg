#pragma once

#include "nutmeg/debugview/render_state.h"

namespace nutmeg::debugview {

// A scripted, simulation-free scene: players jog around their formation spots
// and the ball is passed from player to player.
RenderState demoScene(double seconds);

}
