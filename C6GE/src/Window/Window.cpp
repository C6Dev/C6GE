#include "Window.h"

GLFWwindow* window = nullptr;

namespace C6GE {
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

	void UpdateWindow() { glfwPollEvents(); }

	// Check if the window is open and not closed
	bool IsWindowOpen() { return window && !glfwWindowShouldClose(window); }

	void DestroyWindow() {
		if (window) glfwDestroyWindow(window);
		glfwTerminate();
	}

	// Get the GLFW window pointer
	GLFWwindow* GetWindow() {	return window;	}
}