#pragma once
#include <GLFW/glfw3.h>

namespace C6GE {
class DeltaTime {
public:
    static double deltaTime() {
        static double lastTime = glfwGetTime();
        double currentTime = glfwGetTime();
        double deltaTime = currentTime - lastTime;
        lastTime = currentTime;
        return deltaTime;
    }

    static double GetTime() {
        return glfwGetTime();
    }
};
}