#include <iostream>
#include <thread>
#include <chrono>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#if defined(__APPLE__)
#    define GLFW_EXPOSE_NATIVE_COCOA
#endif
#include <GLFW/glfw3native.h>

#include "DiligentCore/Common/interface/RefCntAutoPtr.hpp"
#include "DiligentCore/Graphics/GraphicsEngine/interface/EngineFactory.h"
#include "DiligentCore/Graphics/GraphicsEngine/interface/SwapChain.h"

#if defined(_WIN32)
#    include "DiligentCore/Graphics/GraphicsEngineD3D12/interface/EngineFactoryD3D12.h"
#    include "DiligentCore/Platforms/Win32/interface/Win32NativeWindow.h"
#elif defined(__APPLE__) || defined(__linux__)
#    include "DiligentCore/Graphics/GraphicsEngineVk/interface/EngineFactoryVk.h"
#endif

using namespace Diligent;

int main()
{
    if (!glfwInit())
    {
        std::cerr << "Failed to init GLFW\n";
        return -1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    const int width = 1280, height = 720;
    GLFWwindow* window = glfwCreateWindow(width, height, "Diligent + GLFW", nullptr, nullptr);
    if (!window)
    {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }

    RefCntAutoPtr<IRenderDevice>  pDevice;
    RefCntAutoPtr<IDeviceContext> pImmediateContext;
    RefCntAutoPtr<ISwapChain>     pSwapChain;

    SwapChainDesc SCDesc{};
    SCDesc.Width  = width;
    SCDesc.Height = height;
    SCDesc.BufferCount = 2;
    SCDesc.ColorBufferFormat = TEX_FORMAT_RGBA8_UNORM;
    SCDesc.DepthBufferFormat = TEX_FORMAT_D32_FLOAT;

#if defined(_WIN32)
    Win32NativeWindow Win32Wnd{};
    Win32Wnd.hWnd = glfwGetWin32Window(window);
    auto* pFactoryD3D12 = GetEngineFactoryD3D12();
    EngineD3D12CreateInfo EngCI{};
    pFactoryD3D12->CreateDeviceAndSwapChainD3D12(
        EngCI, &pDevice, &pImmediateContext, SCDesc, FullScreenModeDesc{}, Win32Wnd, &pSwapChain);

#elif defined(__APPLE__) || defined(__linux__)
    struct VulkanNativeWindow
    {
        void* Window;
        void* pDisplay;
    };

    VulkanNativeWindow VkWnd{};
    VkWnd.Window = reinterpret_cast<void*>(glfwGetCocoaWindow(window));
    VkWnd.pDisplay = nullptr; // Not used on macOS

    auto* pFactoryVk = GetEngineFactoryVk();
    EngineVkCreateInfo EngCI{};
    pFactoryVk->CreateDeviceAndSwapChainVk(EngCI, &pDevice, &pImmediateContext, SCDesc, VkWnd, &pSwapChain);
#endif

    if (!pDevice || !pSwapChain)
    {
        std::cerr << "Failed to create device or swap chain\n";
        return -1;
    }

    std::cout << "Device and swap chain created successfully!\n";

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        pSwapChain->Present();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
