#include "Window.h"

#include <iostream>
#include <stdexcept>
#include <vector>

GLFWwindow* Window::CreateWindow(int width, int height, const char* title) {
    if(!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    GLFWwindow* window = glfwCreateWindow(width, height, title, nullptr, nullptr);

    if(!window) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }

    glfwMakeContextCurrent(window);

    return window;
}

void Window::DestroyWindow(GLFWwindow* window) {
    if(window) {
        glfwDestroyWindow(window);
        glfwTerminate();
    }
}

std::vector<const char*> Window::GetPlatformRequiredInstanceExtensions() {
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    return std::vector<const char*>(glfwExtensions, glfwExtensions + glfwExtensionCount);
}