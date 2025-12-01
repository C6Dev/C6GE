#pragma once

#include <GLFW/glfw3.h>
#include <vector>

class Window {
    public:
        GLFWwindow* CreateWindow(int width, int height, const char* title);

        bool WhileOpen(GLFWwindow* window) { while (!glfwWindowShouldClose(window)) { return true; } return false; }

        void PollEvents() { glfwPollEvents(); }

        void DestroyWindow(GLFWwindow* window);

        std::vector<const char*> GetPlatformRequiredInstanceExtensions();
};