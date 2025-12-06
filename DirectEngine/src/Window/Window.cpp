#include "../../include/Window/Window.h"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

// The CreateWindow function initializes GLFW, sets window hints, creates a window, and makes its context current.
GLFWwindow* Window::CreateWindow(int width, int height, const char* title) {
    // Initialize GLFW
    if(!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW");
    }
    
    // Set GLFW window hints for Vulkan or Metal
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // No default OpenGL context
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE); // Disable window resizing

    GLFWwindow* window = glfwCreateWindow(width, height, title, nullptr, nullptr); // Create the GLFW window

    // Check if window creation was successful
    if(!window) {
        glfwTerminate(); // Terminate GLFW on failure
        throw std::runtime_error("Failed to create GLFW window");
    }

    glfwMakeContextCurrent(window); // Make the window's context current

    return window;
}

// The WhileOpen function checks if the window should remain open.
bool Window::WhileOpen(GLFWwindow* window) {
    return window && !glfwWindowShouldClose(window);
}

// The PollEvents function processes all pending events.
void Window::PollEvents() {
    glfwPollEvents(); // Process all pending events
}

// The DestroyWindow function destroys the window and terminates GLFW.
void Window::DestroyWindow(GLFWwindow* window) {
    if(window) {
        glfwDestroyWindow(window); // Destroy the GLFW window
        glfwTerminate(); // Terminate GLFW
    }
}