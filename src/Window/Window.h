#pragma once

#include <GLFW/glfw3.h>
#include <iostream>

// Platform-specific native access macros and includes
#if defined(_WIN32)
#   define GLFW_EXPOSE_NATIVE_WIN32
#   include <GLFW/glfw3native.h>
#elif defined(__APPLE__)
#   define GLFW_EXPOSE_NATIVE_COCOA
#   include <GLFW/glfw3native.h>
#elif defined(__linux__)
#   define GLFW_EXPOSE_NATIVE_X11
#   include <GLFW/glfw3native.h>
#endif

namespace C6GE {
    class Window {
        private:
            GLFWwindow* m_window;
        public:
            Window() : m_window(nullptr) {}
            bool Init();
            bool CreateGLFWWindow(const char* title, int width, int height);
            GLFWwindow* GetWindow() const { return m_window; }
            void ShowWindow();
            void FocusWindow();
            void HandleWindowEvents();
            void SetFramebufferSizeCallback(GLFWframebuffersizefun callback);
            void* GetWin32Window();
            void* GetCocoaWindow();
            void* GetX11Window();
            void* GetX11Display();
            bool WindowShouldClose();
            void SetWindowShouldClose(bool value);
            int GetWindowAttrib(int attrib);
            void DestroyWindow();
    };
}