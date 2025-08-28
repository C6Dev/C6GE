#include "Window.h"

namespace C6GE {
    bool Window::Init(){
        if (!glfwInit()) {
            std::cout << "Failed to initialize GLFW" << std::endl;
            return false;
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        return true;
    }

    bool Window::CreateGLFWWindow(const char* title, int width, int height){
        m_window = glfwCreateWindow(width, height, title, nullptr, nullptr);
        if (!m_window) {
            std::cout << "Failed to create GLFW window" << std::endl;
            return false;
        }

        glfwMakeContextCurrent(m_window);
        return true;
    }

    void Window::ShowWindow(){
        glfwShowWindow(m_window);
    }

    void Window::FocusWindow(){
        glfwFocusWindow(m_window);
    }

    void Window::HandleWindowEvents(){
        glfwPollEvents();
    }

    void Window::SetFramebufferSizeCallback(GLFWframebuffersizefun callback){
        glfwSetFramebufferSizeCallback(m_window, callback);
    }

    #if defined(_WIN32)
    void* Window::GetWin32Window(){
        return glfwGetWin32Window(m_window);
    }
    #else
    void* Window::GetWin32Window(){
        return nullptr;
    }
    #endif

    #if defined(__APPLE__)
    void* Window::GetCocoaWindow(){
        return glfwGetCocoaWindow(m_window);
    }
    #else
    void* Window::GetCocoaWindow(){
        return nullptr;
    }
    #endif

    #if defined(__linux__)
    void* Window::GetX11Window(){
        return (void*)(uintptr_t)glfwGetX11Window(m_window);
    }

    void* Window::GetX11Display(){
        return glfwGetX11Display();
    }
    #else
    void* Window::GetX11Window(){
        return nullptr;
    }

    void* Window::GetX11Display(){
        return nullptr;
    }
    #endif

    bool Window::WindowShouldClose(){
        return glfwWindowShouldClose(m_window);
    }

    void Window::SetWindowShouldClose(bool value){
        glfwSetWindowShouldClose(m_window, value);
    }

    int Window::GetWindowAttrib(int attrib){
        return glfwGetWindowAttrib(m_window, attrib);
    }

    void Window::DestroyWindow(){
        glfwDestroyWindow(m_window);
        glfwTerminate();
    }

    // make a function to get the window width and height
    int Window::GetWindowWidth(){
        int width, height;
        glfwGetWindowSize(m_window, &width, nullptr);
        return width;
    }

    int Window::GetWindowHeight(){
        int width, height;
        glfwGetWindowSize(m_window, nullptr, &height);
        return height;
    }
    
    // make a function to get the framebuffer width and height
    int Window::GetFramebufferWidth(){
        int width, height;
        glfwGetFramebufferSize(m_window, &width, nullptr);
        return width;
    }

    int Window::GetFramebufferHeight(){
        int width, height;
        glfwGetFramebufferSize(m_window, nullptr, &height);
        return height;
    }
}