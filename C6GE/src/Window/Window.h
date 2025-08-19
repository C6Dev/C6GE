#pragma once
#ifdef _WIN32
#include <windows.h>
#endif

// Always include GLFW for other platforms
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace C6GE {
	#ifdef _WIN32
#undef CreateWindow
#undef GetWindow
#endif

bool CreateWindow(int width, int height, const char* title);
void UpdateWindow();
bool IsWindowOpen();
void DestroyWindow();

// Return void* to handle both GLFWwindow* and NSWindow*
void* GetWindow();

float GetFOV();
void SetFOV(float newFOV);
float GetNearPlane();
void SetNearPlane(float newNear);
float GetFarPlane();
void SetFarPlane(float newFar);
glm::mat4 GetProjectionMatrix();

#ifdef __APPLE__
bool CreateWindow_macOS(int width, int height, const char* title);
void UpdateWindow_macOS();
bool IsWindowOpen_macOS();
void DestroyWindow_macOS();
void* GetWindow_macOS();
#endif
}