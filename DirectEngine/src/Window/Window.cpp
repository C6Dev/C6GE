#include "../../include/Window/Window.h"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace DirectLogger;

// The CreateWindow function initializes GLFW, sets window hints, creates a window, and makes its context current.
GLFWwindow* Window::CreateWindow(int width, int height, const char* title) {
    // Initialize GLFW
    if(!glfwInit()) {
        Log(LogLevel::critical, "Failed to initialize GLFW", "Window");
        throw;
    }
    
    // Set GLFW window hints for Vulkan or Metal
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    GLFWwindow* window = glfwCreateWindow(width, height, title, nullptr, nullptr);

    // Check if window creation was successful
    if(!window) {
        glfwTerminate();
        Log(LogLevel::critical, "Failed to create GLFW window", "Window");
        throw;
    }

    glfwMakeContextCurrent(window);

    return window;
}

// The WhileOpen function checks if the window should remain open.
bool Window::WhileOpen(GLFWwindow* window) {
    return window && !glfwWindowShouldClose(window);
}

// The PollEvents function processes all pending events.
void Window::PollEvents() {
    glfwPollEvents();
}

// The DestroyWindow function destroys the window and terminates GLFW.
void Window::DestroyWindow(GLFWwindow* window) {
    if(window) {
        glfwDestroyWindow(window);
        glfwTerminate();
    }
}