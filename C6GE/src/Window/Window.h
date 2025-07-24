#pragma once
#include <GLFW/glfw3.h>

namespace C6GE {
	#ifdef _WIN32
#undef CreateWindow
#endif
bool CreateWindow(int width, int height, const char* title);
	void UpdateWindow();
	bool IsWindowOpen();
	void DestroyWindow();
}