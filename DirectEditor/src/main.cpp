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

    auto window = windowManager.CreateWindow(800, 600, "Direct Editor Window");

    renderPipeline.CreateRender(RenderType);

    projectsManager.CreateProject("MyNewProject", "./Projects", "DefaultTemplate");

    projectsManager.OpenProject("./Projects/MyNewProject/MyNewProject.deproj", "MyNewProject");

    projectsManager.BuildProject("./Projects/MyNewProject", "MyNewProject", "MyNewProject");

    while(windowManager.WhileOpen(window)) {
        windowManager.PollEvents();
    }

    projectsManager.CloseProject();
    renderPipeline.CleanupRenderer(RenderType);
    windowManager.DestroyWindow(window);
    return 0;
}