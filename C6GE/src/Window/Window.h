#pragma once
#ifdef _WIN32
#include <windows.h>
#endif
#include <GLFW/glfw3.h>

namespace C6GE {
	#ifdef _WIN32
#undef CreateWindow
#undef GetWindow
#endif
bool CreateWindow(int width, int height, const char* title);
	void UpdateWindow();
	bool IsWindowOpen();
	void DestroyWindow();
	GLFWwindow* GetWindow();
}