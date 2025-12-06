#pragma once

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#ifdef CreateWindow
#undef CreateWindow
#endif
#endif

#include <GLFW/glfw3.h>
#include <vector>

#include "../main.h"

struct GLFWwindow;

class DirectEngine_API Window {
public:
    GLFWwindow* CreateWindow(int width, int height, const char* title);
    bool WhileOpen(GLFWwindow* window);
    void PollEvents();
    void DestroyWindow(GLFWwindow* window);
};