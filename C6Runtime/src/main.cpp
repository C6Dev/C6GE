#include "Window.h"
#include "Render/RenderPicker.h"
#include "Render/RenderPipeline.h"

#include <iostream>

int main() {
    Window windowManager;
    RenderPicker renderPicker;

    auto RenderType = RenderPicker::GetRenderType();

    if (RenderType == RenderPicker::RenderType::Vulkan) {
        std::cout << "Render Type: Vulkan" << std::endl;
    } else if (RenderType == RenderPicker::RenderType::Metal) {
        std::cout << "Render Type: Metal" << std::endl;
    } else {
        std::cout << "Render Type: Unknown" << std::endl;
    }

    RenderPipeline renderPipeline;

    auto window = windowManager.CreateWindow(800, 600, "C6 Runtime Window");

    auto extensions = windowManager.GetPlatformRequiredInstanceExtensions();

    renderPipeline.CreateInstance(RenderType, extensions);

    while(windowManager.WhileOpen(window)) {
        windowManager.PollEvents();
    }

    renderPipeline.CleanupRenderer(RenderType);
    windowManager.DestroyWindow(window);
    return 0;
}