#include "Window.h"

namespace C6GE {

GLFWwindow* window = nullptr;

float fov = 60.0f;
float nearPlane = 0.1f;
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
		if (!glfwInit()) return false; // return false if GLFW initialization fails

		// Set OpenGL version to 3.3
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

		// Set OpenGL profile to core
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

		window = glfwCreateWindow(width, height, title, nullptr, nullptr);

		// Check if window creation was successful
		if (!window) {
			glfwTerminate();
			return false;
		}

		glfwMakeContextCurrent(window);
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
	bool IsWindowOpen() { return window && !glfwWindowShouldClose(window); }

	void DestroyWindow() {
		if (window) glfwDestroyWindow(window);
		glfwTerminate();
	}

	// Get the GLFW window pointer
	GLFWwindow* GetWindow() {	return window;	}
}