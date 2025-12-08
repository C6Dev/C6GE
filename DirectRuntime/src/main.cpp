#include "Window/Window.h"
#include "Render/RenderPicker.h"
#include "Render/RenderPipeline.h"
#include "Projects/Projects.h"

#include <iostream>

int main() {
    Window windowManager;
    RenderPicker renderPicker;
    Projects projectsManager;

    auto RenderType = RenderPicker::GetRenderType();

    RenderPipeline renderPipeline;

    auto window = windowManager.CreateWindow(800, 600, "Direct Runtime Window");

    renderPipeline.CreateRender(RenderType);

    // Find and open any .deproj file that's in the same folder as the executable
    std::filesystem::path exePath = std::filesystem::current_path();
    bool projectOpened = false;
    for (const auto& entry : std::filesystem::directory_iterator(exePath)) {
        if (entry.path().extension() == ".deproj") {
            projectsManager.OpenProject(entry.path().string(), entry.path().stem().string());
            projectOpened = true;
            break;
        }
    }
    if (!projectOpened) {
        throw std::runtime_error("No .deproj file found in the executable directory.");
    }
    
    while(windowManager.WhileOpen(window)) {
        windowManager.PollEvents();
    }

    renderPipeline.CleanupRenderer(RenderType);
    windowManager.DestroyWindow(window);
    return 0;
}