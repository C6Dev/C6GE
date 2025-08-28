#pragma once

#include <bgfx/bgfx.h>
#include <bgfx/platform.h>

#include "../Window/Window.h"

namespace C6GE {

    class Render {
        public:
        void RenderFrame();
        void SetPlatformData(C6GE::Window& window, bgfx::PlatformData& pd);
        bool Init(C6GE::Window& window, bgfx::PlatformData& pd);
        void UpdateWindowSize(C6GE::Window& window);
    };
}