#include <glad/glad.h>
#include "Window.h"
#include "../Logging/Log.h"

namespace C6GE {

#ifdef __APPLE__
// macOS: Use GLFW window for now (we'll call macOS-specific functions)
GLFWwindow* window = nullptr;
#else
// Other platforms: Use GLFW window
GLFWwindow* window = nullptr;
#endif

float fov = 60.0f;
float nearPlane = 0.3f;
float farPlane = 100.0f;
glm::mat4 projectionMatrix;

float GetFOV() {
    return fov;
}

void SetFOV(float newFOV) {
    fov = newFOV;
}

float GetNearPlane() {
    return nearPlane;
}

void SetNearPlane(float newNear) {
    nearPlane = newNear;
}

float GetFarPlane() {
    return farPlane;
}

void SetFarPlane(float newFar) {
    farPlane = newFar;
}

glm::mat4 GetProjectionMatrix() {
    return projectionMatrix;
}

#ifdef _WIN32
#undef CreateWindow
#endif

bool CreateWindow(int width, int height, const char* title) {
    if (!glfwInit()) return false;

    // Set OpenGL version to 3.3
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    glfwWindowHint(GLFW_DEPTH_BITS, 24);
    window = glfwCreateWindow(width, height, title, nullptr, nullptr);

    // Check if window creation was successful
    if (!window) {
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(0);
    return true;
}

void UpdateWindow() {
    glfwPollEvents();

    int framebufferWidth, framebufferHeight;
    glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);

    glViewport(0, 0, framebufferWidth, framebufferHeight);

    float aspect = static_cast<float>(framebufferWidth) / framebufferHeight;
    projectionMatrix = glm::perspective(glm::radians(fov), aspect, nearPlane, farPlane);
}

// Check if the window is open and not closed
bool IsWindowOpen() { 
    return window && !glfwWindowShouldClose(window); 
}

void DestroyWindow() {
    if (window) glfwDestroyWindow(window);
    glfwTerminate();
}

// Get the window pointer
void* GetWindow() { 
    return window; 
}
}