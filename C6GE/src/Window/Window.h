#pragma once
#ifdef _WIN32
#include <windows.h>
#endif
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
	GLFWwindow* GetWindow();

float GetFOV();
void SetFOV(float newFOV);
float GetNearPlane();
void SetNearPlane(float newNear);
float GetFarPlane();
void SetFarPlane(float newFar);
glm::mat4 GetProjectionMatrix();
}