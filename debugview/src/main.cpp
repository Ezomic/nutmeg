#include "nutmeg/core/log.h"
#include "nutmeg/debugview/debug_renderer.h"
#include "nutmeg/debugview/demo_scene.h"
#include "nutmeg/platform/sdl_platform.h"

#include <SDL3/SDL_scancode.h>

#include <cstdlib>
#include <cstring>

int main(int argc, char** argv)
{
    using namespace nutmeg;

    double autoQuitSeconds = 0.0;
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::strcmp(argv[i], "--seconds") == 0) {
            autoQuitSeconds = std::atof(argv[i + 1]);
        }
    }

    SdlPlatform platform;
    WindowDesc desc;
    desc.title = "Nutmeg debug view";
    if (!platform.init(desc)) {
        return 1;
    }
    debugview::DebugRenderer renderer;
    if (!renderer.init(platform.window())) {
        return 1;
    }

    debugview::Overlays overlays;
    const double start = platform.now();
    while (!platform.quitRequested()) {
        platform.pumpEvents();
        if (platform.keyPressed(SDL_SCANCODE_N)) overlays.numbers = !overlays.numbers;
        if (platform.keyPressed(SDL_SCANCODE_F)) overlays.facing = !overlays.facing;
        if (platform.keyPressed(SDL_SCANCODE_T)) overlays.text = !overlays.text;
        if (platform.keyPressed(SDL_SCANCODE_G)) overlays.grid = !overlays.grid;

        const double elapsed = platform.now() - start;
        if (autoQuitSeconds > 0.0 && elapsed >= autoQuitSeconds) {
            platform.requestQuit();
        }
        renderer.draw(debugview::demoScene(elapsed), overlays);
    }
    return 0;
}
