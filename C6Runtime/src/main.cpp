#include "Window.h"

int main() {
    Window windowManager;

    auto window = windowManager.CreateWindow(800, 600, "C6 Runtime Window");

    while(windowManager.WhileOpen(window)) {
        windowManager.PollEvents();
    }

    windowManager.DestroyWindow(window);
    return 0;
}