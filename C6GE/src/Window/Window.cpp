#include "Window.h"

GLFWwindow* window = nullptr;

namespace C6GE {
	bool CreateWindow(int width, int height, const char* title) {
		// Check if GLFW is initialized
		if (!glfwInit()) return false;

		// Set OpenGL version to 3.3
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

		// Set OpenGL profile to core
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

		// Create a window
		window = glfwCreateWindow(width, height, title, nullptr, nullptr);

		// Check if window creation was successful
		if (!window) {
			glfwTerminate();
			return false;
		}

		glfwMakeContextCurrent(window);
		return true;
	}

	// Update the window by polling events
	void UpdateWindow() { glfwPollEvents(); }

	// Check if the window is open and not closed
	bool IsWindowOpen() { return window && !glfwWindowShouldClose(window); }

	// Destroy the window and terminate GLFW
	void DestroyWindow() {
		if (window) glfwDestroyWindow(window);
		glfwTerminate();
	}

	// Get the GLFW window pointer
	GLFWwindow* GetWindow() {	return window;	}
}