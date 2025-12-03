#include "Window.h"

#include <cstdint>
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

bool Window::WhileOpen(GLFWwindow* window) {
    return window && !glfwWindowShouldClose(window);
}

void Window::PollEvents() {
    glfwPollEvents();
}

void Window::DestroyWindow(GLFWwindow* window) {
    if(window) {
        glfwDestroyWindow(window);
        glfwTerminate();
    }
}