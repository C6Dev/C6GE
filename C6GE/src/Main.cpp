// Main.cpp
// V0.1
// this supported platforms list will be removed once all platforms are tested and working
// Supoorted Platforms:
// - Windows Tested Works Render = OpenGL 3.3 Best Render = DirectX 12/Vulkan
// - Linux Untested Should Work ( just with cmake ) Render = OpenGL 3.3 Best Render = Vulkan
// - MacOS Untested Should Work ( with tweak to make cmake and posibley other things work ) Render = OpenGL 3.3 Best Render = Metal
//
// This file is part of the C6GE project, which is licensed under the MIT License.

#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

int main() {

	// Initialize GLFW
	glfwInit();

	// Set OpenGL version to 3.3
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	// Set OpenGL profile to core
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// Create a windowed mode window and its OpenGL context
	GLFWwindow* window = glfwCreateWindow(800, 800, "C6GE Window", nullptr, nullptr);

	if (window == nullptr) 
	{
		std::cerr << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}

	// Make the window's context current
	glfwMakeContextCurrent(window);

	// Initialize GLAD to configure OpenGL
	gladLoadGL();

	// Set the viewport to the size of the window
	glViewport(0, 0, 800, 800);

	// sets background color to dark grey
	glClearColor(0.2f, 0.3f, 0.3f, 1.0f);

	// Clear the color buffer
	glClear(GL_COLOR_BUFFER_BIT);

	// Swap the front and back buffers
	glfwSwapBuffers(window);

	// Keep Window open until it should close and Update Styff
	while (!glfwWindowShouldClose(window)) 
	{
		glfwPollEvents();
	}

	// Clean up and exit
	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}