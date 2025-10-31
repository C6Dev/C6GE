#include <iostream>
#include <thread>
#include <chrono>
#include <memory>
#include <random>
#include <string>
#include <algorithm>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#if defined(_WIN32)
    #ifndef GLFW_EXPOSE_NATIVE_WIN32
        #define GLFW_EXPOSE_NATIVE_WIN32
    #endif
    #include <GLFW/glfw3native.h>
    #include "DiligentCore/Platforms/Win32/interface/Win32NativeWindow.h"
    #include <Windows.h>

#elif defined(__APPLE__)
    #ifndef GLFW_EXPOSE_NATIVE_COCOA
        #define GLFW_EXPOSE_NATIVE_COCOA
    #endif
    #include <GLFW/glfw3native.h> 
    #include "DiligentCore/Platforms/Apple/interface/MacOSNativeWindow.h"
    #include <Cocoa/Cocoa.h>
    #include <QuartzCore/CAMetalLayer.h>

#elif defined(__linux__)
    #ifndef GLFW_EXPOSE_NATIVE_X11
        #define GLFW_EXPOSE_NATIVE_X11
    #endif
    #include <GLFW/glfw3native.h>
    // Undefine X11 macros that conf        // Set DisplaySize and scale for DPI scaling
    #ifdef Bool
    #undef Bool
    #endif
    #ifdef True
    #undef True  
    #endif
    #ifdef False
    #undef False
    #endif
    #ifdef None
    #undef None
    #endif
    #include "DiligentCore/Platforms/Linux/interface/LinuxNativeWindow.h"
#endif

// Diligent common interfaces
#include "DiligentCore/Common/interface/RefCntAutoPtr.hpp"
#include "DiligentCore/Graphics/GraphicsEngine/interface/EngineFactory.h"
#include "DiligentCore/Graphics/GraphicsEngine/interface/SwapChain.h"
#include "DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h"
#include "DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h"
#include "DiligentCore/Graphics/GraphicsEngine/interface/Shader.h"
#include "DiligentCore/Graphics/GraphicsEngine/interface/PipelineState.h"

// Engine factories
#include "DiligentCore/Graphics/GraphicsEngineD3D12/interface/EngineFactoryD3D12.h"
#include "DiligentCore/Graphics/GraphicsEngineD3D11/interface/EngineFactoryD3D11.h"
#include "DiligentCore/Graphics/GraphicsEngineVulkan/interface/EngineFactoryVk.h"
#include "DiligentCore/Graphics/GraphicsEngineOpenGL/interface/EngineFactoryOpenGL.h"
#ifdef __APPLE__
#include "DiligentCore/Graphics/GraphicsEngineMetal/interface/EngineFactoryMtl.h"
#endif

// Your includes
#include "Render/Render.h"
#include "Runtime/ECS/World.h"
#include "Runtime/ECS/Components.h"
#include "MapHelper.hpp"
#include "GraphicsUtilities.h"
#include "ShaderMacroHelper.hpp"
#include "TextureUtilities.h"
#include "ColorConversion.h"
#include "../../Common/src/TexturedCube.hpp"
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "ImGuiUtils.hpp"
#include "ImGuiImplDiligent.hpp"
#include "Align.hpp"
#include "FirstPersonCamera.hpp"
#include "FastRand.hpp"
#include "Bloom.hpp"
#include "SuperResolution.hpp"
#include "ShaderSourceFactoryUtils.hpp"
#include "PostFXContext.hpp"
#include "ScopedDebugGroup.hpp"
#include "TemporalAntiAliasing.hpp"
#include "ScreenSpaceReflection.hpp"
#include "ScreenSpaceAmbientOcclusion.hpp"
#include "Utilities/interface/DiligentFXShaderSourceStreamFactory.hpp"
#include "CommonlyUsedStates.h"
#include "RenderStateCache.hpp"
#include "GraphicsTypesX.hpp"
#include "GBuffer.hpp"
#include "EnvMapRenderer.hpp"


// Use unified input controller that aliases to the platform-specific implementation
#include "DiligentSamples/SampleBase/include/InputController.hpp"
#if defined(__APPLE__)
#include "../external/DiligentEngine/DiligentSamples/SampleBase/include/MacOS/InputControllerMacOS.hpp"
#endif

#include "EditorTheme/EditorTheme.h"


using namespace Diligent;

namespace
{

struct NativeWindowWrapper
{
#if defined(_WIN32)
    Win32NativeWindow win32{};
#elif defined(__APPLE__)
    MacOSNativeWindow mac{};
#elif defined(__linux__)
    LinuxNativeWindow x11{};
#endif
};

static NativeWindowWrapper GetNativeWindow(GLFWwindow* window)
{
    NativeWindowWrapper native{};
#if defined(_WIN32)
    native.win32.hWnd = glfwGetWin32Window(window);
#elif defined(__APPLE__)
    native.mac.pNSView = [glfwGetCocoaWindow(window) contentView];
#elif defined(__linux__)
    native.x11.WindowId = glfwGetX11Window(window);
    native.x11.pDisplay = glfwGetX11Display();
#endif
    return native;
}

static NativeWindowWrapper GetNativeWindow(ImGuiViewport* viewport)
{
    if (viewport == nullptr || viewport->PlatformHandle == nullptr)
        return {};
    return GetNativeWindow(static_cast<GLFWwindow*>(viewport->PlatformHandle));
}

struct DiligentViewportData
{
    RefCntAutoPtr<ISwapChain> SwapChain;
};

bool g_ImGuiGlfwBackendEnabled = false;
RefCntAutoPtr<IRenderDevice> g_RenderDevice;
RefCntAutoPtr<IDeviceContext> g_ImmediateContext;
RefCntAutoPtr<ISwapChain> g_MainSwapChain;
#if defined(_WIN32)
#    if D3D11_SUPPORTED
RefCntAutoPtr<IEngineFactoryD3D11> g_FactoryD3D11;
#    endif
#    if D3D12_SUPPORTED
RefCntAutoPtr<IEngineFactoryD3D12> g_FactoryD3D12;
#    endif
#endif
RENDER_DEVICE_TYPE g_DeviceType = RENDER_DEVICE_TYPE_UNDEFINED;
ImGuiImplDiligent* g_ImGuiRenderer = nullptr;

static bool InitializeDiligentViewportGlobals(const RefCntAutoPtr<IRenderDevice>& device,
                                              const RefCntAutoPtr<IDeviceContext>& context,
                                              const RefCntAutoPtr<ISwapChain>& swapChain,
                                              const RefCntAutoPtr<IEngineFactory>& factory)
{
    g_RenderDevice = device;
    g_ImmediateContext = context;
    g_MainSwapChain = swapChain;
    g_DeviceType = device ? device->GetDeviceInfo().Type : RENDER_DEVICE_TYPE_UNDEFINED;

#if defined(_WIN32)
#    if D3D11_SUPPORTED
    g_FactoryD3D11.Release();
#    endif
#    if D3D12_SUPPORTED
    g_FactoryD3D12.Release();
#    endif
    if (g_DeviceType == RENDER_DEVICE_TYPE_D3D11)
    {
#    if D3D11_SUPPORTED
        g_FactoryD3D11 = RefCntAutoPtr<IEngineFactoryD3D11>{factory, IID_EngineFactoryD3D11};
        return g_FactoryD3D11 != nullptr;
#    else
        return false;
#    endif
    }
    else if (g_DeviceType == RENDER_DEVICE_TYPE_D3D12)
    {
#    if D3D12_SUPPORTED
        g_FactoryD3D12 = RefCntAutoPtr<IEngineFactoryD3D12>{factory, IID_EngineFactoryD3D12};
        return g_FactoryD3D12 != nullptr;
#    else
        return false;
#    endif
    }
#endif

    return false;
}

static void ImGui_Diligent_DestroyWindow(ImGuiViewport* viewport);

static void ImGui_Diligent_CreateWindow(ImGuiViewport* viewport)
{
    if (!viewport)
        return;
    if (!g_RenderDevice || !g_ImmediateContext || !g_MainSwapChain)
        return;

    auto* data = IM_NEW(DiligentViewportData)();
    viewport->RendererUserData = data;

    SwapChainDesc desc = g_MainSwapChain->GetDesc();
    desc.Width = static_cast<Uint32>(std::max(1.0f, viewport->Size.x));
    desc.Height = static_cast<Uint32>(std::max(1.0f, viewport->Size.y));

    bool created = false;
#if defined(_WIN32)
    const auto native = GetNativeWindow(viewport);
    const FullScreenModeDesc fsDesc{};
    if (g_DeviceType == RENDER_DEVICE_TYPE_D3D11)
    {
#    if D3D11_SUPPORTED
        if (g_FactoryD3D11)
        {
            g_FactoryD3D11->CreateSwapChainD3D11(g_RenderDevice, g_ImmediateContext, desc, fsDesc, native.win32, &data->SwapChain);
            created = (data->SwapChain != nullptr);
        }
#    endif
    }
    else if (g_DeviceType == RENDER_DEVICE_TYPE_D3D12)
    {
#    if D3D12_SUPPORTED
        if (g_FactoryD3D12)
        {
            g_FactoryD3D12->CreateSwapChainD3D12(g_RenderDevice, g_ImmediateContext, desc, fsDesc, native.win32, &data->SwapChain);
            created = (data->SwapChain != nullptr);
        }
#    endif
    }
#endif

    if (!created)
    {
        ImGui_Diligent_DestroyWindow(viewport);
        std::cerr << "[C6GE] Failed to create swap chain for ImGui viewport." << std::endl;
    }
}

static void ImGui_Diligent_DestroyWindow(ImGuiViewport* viewport)
{
    if (viewport == nullptr)
        return;
    if (auto* data = static_cast<DiligentViewportData*>(viewport->RendererUserData))
    {
        data->SwapChain.Release();
        IM_DELETE(data);
    }
    viewport->RendererUserData = nullptr;
}

static void ImGui_Diligent_SetWindowSize(ImGuiViewport* viewport, ImVec2 size)
{
    if (viewport == nullptr)
        return;
    if (auto* data = static_cast<DiligentViewportData*>(viewport->RendererUserData))
    {
        if (data->SwapChain)
        {
            data->SwapChain->Resize(static_cast<Uint32>(std::max(1.0f, size.x)),
                                    static_cast<Uint32>(std::max(1.0f, size.y)));
        }
    }
}

static void ImGui_Diligent_RenderWindow(ImGuiViewport* viewport, void*)
{
    if (viewport == nullptr || g_ImGuiRenderer == nullptr)
        return;
    auto* data = static_cast<DiligentViewportData*>(viewport->RendererUserData);
    if (!data || !data->SwapChain)
        return;

    ITextureView* rtv = data->SwapChain->GetCurrentBackBufferRTV();
    ITextureView* dsv = data->SwapChain->GetDepthBufferDSV();
    if (!rtv)
        return;

    ITextureView* rtvs[] = {rtv};
    g_ImmediateContext->SetRenderTargets(1, rtvs, dsv, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    if (!(viewport->Flags & ImGuiViewportFlags_NoRendererClear))
    {
        const float clearColor[4] = {0.f, 0.f, 0.f, 1.f};
        g_ImmediateContext->ClearRenderTarget(rtv, clearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        if (dsv)
        {
            g_ImmediateContext->ClearDepthStencil(dsv, CLEAR_DEPTH_FLAG, 1.0f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        }
    }

    g_ImGuiRenderer->RenderDrawData(g_ImmediateContext, viewport->DrawData);
}

static void ImGui_Diligent_SwapBuffers(ImGuiViewport* viewport, void*)
{
    if (auto* data = static_cast<DiligentViewportData*>(viewport ? viewport->RendererUserData : nullptr))
    {
        if (data->SwapChain)
            data->SwapChain->Present(0);
    }
}

static void ConfigureImGuiMultiViewport(ImGuiIO& io)
{
    io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    platform_io.Renderer_CreateWindow = ImGui_Diligent_CreateWindow;
    platform_io.Renderer_DestroyWindow = ImGui_Diligent_DestroyWindow;
    platform_io.Renderer_SetWindowSize = ImGui_Diligent_SetWindowSize;
    platform_io.Renderer_RenderWindow = ImGui_Diligent_RenderWindow;
    platform_io.Renderer_SwapBuffers = ImGui_Diligent_SwapBuffers;
}

} // namespace

// Windows-only GLFW callbacks that forward events into Diligent InputControllerWin32
#if defined(_WIN32)
static UINT MapGLFWKeyToVK(int key)
{
    switch (key)
    {
        case GLFW_KEY_LEFT: return VK_LEFT;
        case GLFW_KEY_RIGHT: return VK_RIGHT;
        case GLFW_KEY_UP: return VK_UP;
        case GLFW_KEY_DOWN: return VK_DOWN;
        case GLFW_KEY_A: return 'A';
        case GLFW_KEY_D: return 'D';
        case GLFW_KEY_W: return 'W';
        case GLFW_KEY_S: return 'S';
        case GLFW_KEY_Q: return 'Q';
        case GLFW_KEY_E: return 'E';
        case GLFW_KEY_LEFT_SHIFT: case GLFW_KEY_RIGHT_SHIFT: return VK_SHIFT;
        case GLFW_KEY_LEFT_CONTROL: case GLFW_KEY_RIGHT_CONTROL: return VK_CONTROL;
        case GLFW_KEY_LEFT_ALT: case GLFW_KEY_RIGHT_ALT: return VK_MENU;
        case GLFW_KEY_HOME: return VK_HOME;
        case GLFW_KEY_KP_ADD: case GLFW_KEY_EQUAL: return VK_ADD;
        case GLFW_KEY_KP_SUBTRACT: case GLFW_KEY_MINUS: return VK_SUBTRACT;
        default:
            return 0;
    }
}

static void GLFWKeyCallback(GLFWwindow* w, int key, int scancode, int action, int mods)
{
    ImGuiIO& io = ImGui::GetIO();
    if (!g_ImGuiGlfwBackendEnabled)
    {
        io.AddKeyEvent(ImGuiMod_Ctrl, (mods & GLFW_MOD_CONTROL) != 0);
        io.AddKeyEvent(ImGuiMod_Shift, (mods & GLFW_MOD_SHIFT) != 0);
        io.AddKeyEvent(ImGuiMod_Alt, (mods & GLFW_MOD_ALT) != 0);
        io.AddKeyEvent(ImGuiMod_Super, (mods & GLFW_MOD_SUPER) != 0);

        ImGuiKey imgui_key = ImGuiKey_None;
        switch (key)
        {
            case GLFW_KEY_TAB: imgui_key = ImGuiKey_Tab; break;
            case GLFW_KEY_LEFT: imgui_key = ImGuiKey_LeftArrow; break;
            case GLFW_KEY_RIGHT: imgui_key = ImGuiKey_RightArrow; break;
            case GLFW_KEY_UP: imgui_key = ImGuiKey_UpArrow; break;
            case GLFW_KEY_DOWN: imgui_key = ImGuiKey_DownArrow; break;
            case GLFW_KEY_PAGE_UP: imgui_key = ImGuiKey_PageUp; break;
            case GLFW_KEY_PAGE_DOWN: imgui_key = ImGuiKey_PageDown; break;
            case GLFW_KEY_HOME: imgui_key = ImGuiKey_Home; break;
            case GLFW_KEY_END: imgui_key = ImGuiKey_End; break;
            case GLFW_KEY_INSERT: imgui_key = ImGuiKey_Insert; break;
            case GLFW_KEY_DELETE: imgui_key = ImGuiKey_Delete; break;
            case GLFW_KEY_BACKSPACE: imgui_key = ImGuiKey_Backspace; break;
            case GLFW_KEY_SPACE: imgui_key = ImGuiKey_Space; break;
            case GLFW_KEY_ENTER: imgui_key = ImGuiKey_Enter; break;
            case GLFW_KEY_ESCAPE: imgui_key = ImGuiKey_Escape; break;
            case GLFW_KEY_A: imgui_key = ImGuiKey_A; break;
            case GLFW_KEY_C: imgui_key = ImGuiKey_C; break;
            case GLFW_KEY_V: imgui_key = ImGuiKey_V; break;
            case GLFW_KEY_X: imgui_key = ImGuiKey_X; break;
            case GLFW_KEY_Y: imgui_key = ImGuiKey_Y; break;
            case GLFW_KEY_Z: imgui_key = ImGuiKey_Z; break;
            default: break;
        }

        if (imgui_key != ImGuiKey_None)
        {
            io.AddKeyEvent(imgui_key, (action == GLFW_PRESS || action == GLFW_REPEAT));
        }
    }

    // If ImGui wants keyboard, don't forward to the app
    if (io.WantCaptureKeyboard)
        return;

    auto* samplePtr = static_cast<SampleBase*>(glfwGetWindowUserPointer(w));
    if (!samplePtr)
        return;

    UINT vk = MapGLFWKeyToVK(key);
    if (vk == 0)
        return;

    struct WindowMessageData { HWND hWnd; UINT message; WPARAM wParam; LPARAM lParam; } MsgData{};
    MsgData.hWnd = glfwGetWin32Window(w);
    MsgData.wParam = (WPARAM)vk;
    MsgData.lParam = 0;
    if (action == GLFW_PRESS || action == GLFW_REPEAT)
        MsgData.message = WM_KEYDOWN;
    else if (action == GLFW_RELEASE)
        MsgData.message = WM_KEYUP;
    else
        return;

    // Cast controller to Win32 implementation and call HandleNativeMessage
    auto& winCtrl = reinterpret_cast<Diligent::InputController&>(samplePtr->GetInputController());
    winCtrl.HandleNativeMessage(&MsgData);
}

static void GLFWMouseButtonCallback(GLFWwindow* w, int button, int action, int mods)
{
    ImGuiIO& io = ImGui::GetIO();
    if (!g_ImGuiGlfwBackendEnabled)
    {
        if (button >= 0 && button < ImGuiMouseButton_COUNT)
        {
            io.AddMouseButtonEvent(button, action == GLFW_PRESS);
        }
    }

    if (io.WantCaptureMouse)
        return;

    auto* samplePtr = static_cast<SampleBase*>(glfwGetWindowUserPointer(w));
    if (!samplePtr)
        return;

    double xd, yd;
    glfwGetCursorPos(w, &xd, &yd);
    int x = static_cast<int>(xd);
    int y = static_cast<int>(yd);

    struct WindowMessageData { HWND hWnd; UINT message; WPARAM wParam; LPARAM lParam; } MsgData{};
    MsgData.hWnd = glfwGetWin32Window(w);
    MsgData.lParam = (LPARAM)((y << 16) | (x & 0xFFFF));

    // If ImGui didn't capture, forward the event to app's input controller.
    // Trick: map RIGHT button to LEFT for camera rotation behavior.
    if (button == GLFW_MOUSE_BUTTON_LEFT)
        MsgData.message = (action == GLFW_PRESS) ? WM_LBUTTONDOWN : WM_LBUTTONUP;
    else if (button == GLFW_MOUSE_BUTTON_MIDDLE)
        MsgData.message = (action == GLFW_PRESS) ? WM_MBUTTONDOWN : WM_MBUTTONUP;
    else if (button == GLFW_MOUSE_BUTTON_RIGHT)
        MsgData.message = (action == GLFW_PRESS) ? WM_LBUTTONDOWN : WM_LBUTTONUP;
    else
        return;

    auto& winCtrl2 = reinterpret_cast<Diligent::InputController&>(samplePtr->GetInputController());
    winCtrl2.HandleNativeMessage(&MsgData);
}

static void GLFWCursorPosCallback(GLFWwindow* w, double xpos, double ypos)
{
    ImGuiIO& io = ImGui::GetIO();
    if (!g_ImGuiGlfwBackendEnabled)
    {
        io.AddMousePosEvent(static_cast<float>(xpos), static_cast<float>(ypos));
    }

    if (io.WantCaptureMouse)
        return;

    auto* samplePtr = static_cast<SampleBase*>(glfwGetWindowUserPointer(w));
    if (!samplePtr)
        return;
    // Mouse position/state is handled by the platform input controller; no action needed here.
}

static void GLFWScrollCallback(GLFWwindow* w, double xoffset, double yoffset)
{
    ImGuiIO& io = ImGui::GetIO();
    if (!g_ImGuiGlfwBackendEnabled)
    {
        io.AddMouseWheelEvent(static_cast<float>(xoffset), static_cast<float>(yoffset));
    }

    if (io.WantCaptureMouse)
        return;

    auto* samplePtr = static_cast<SampleBase*>(glfwGetWindowUserPointer(w));
    if (!samplePtr)
        return;

    struct WindowMessageData { HWND hWnd; UINT message; WPARAM wParam; LPARAM lParam; } MsgData{};
    MsgData.hWnd = glfwGetWin32Window(w);
    short delta = static_cast<short>(yoffset * WHEEL_DELTA);
    MsgData.wParam = (WPARAM)((UINT)delta << 16);
    MsgData.message = WM_MOUSEWHEEL;
    MsgData.lParam = 0;
    auto& winCtrl3 = reinterpret_cast<Diligent::InputController&>(samplePtr->GetInputController());
    winCtrl3.HandleNativeMessage(&MsgData);
}

static void GLFWCharCallback(GLFWwindow* w, unsigned int c)
{
    ImGuiIO& io = ImGui::GetIO();
    if (!g_ImGuiGlfwBackendEnabled)
    {
        io.AddInputCharacter(c);
    }
}
#endif

// Character callback for Linux
#if defined(__linux__)
static void GLFWCharCallbackLinux(GLFWwindow* w, unsigned int c)
{
    ImGuiIO& io = ImGui::GetIO();
    #include <GLFW/glfw3native.h> 
}
#endif

#if defined(__APPLE__)
// macOS-specific GLFW callbacks that forward events into Diligent InputControllerMacOS
static void GLFWKeyCallbackMac(GLFWwindow* w, int key, int scancode, int action, int mods)
{
    ImGuiIO& io = ImGui::GetIO();
    if (!g_ImGuiGlfwBackendEnabled)
    {
        io.AddKeyEvent(ImGuiMod_Ctrl, (mods & GLFW_MOD_CONTROL) != 0);
        io.AddKeyEvent(ImGuiMod_Shift, (mods & GLFW_MOD_SHIFT) != 0);
        io.AddKeyEvent(ImGuiMod_Alt, (mods & GLFW_MOD_ALT) != 0);
        io.AddKeyEvent(ImGuiMod_Super, (mods & GLFW_MOD_SUPER) != 0);

        ImGuiKey imgui_key = ImGuiKey_None;
        switch (key)
        {
            case GLFW_KEY_TAB: imgui_key = ImGuiKey_Tab; break;
            case GLFW_KEY_LEFT: imgui_key = ImGuiKey_LeftArrow; break;
            case GLFW_KEY_RIGHT: imgui_key = ImGuiKey_RightArrow; break;
            case GLFW_KEY_UP: imgui_key = ImGuiKey_UpArrow; break;
            case GLFW_KEY_DOWN: imgui_key = ImGuiKey_DownArrow; break;
            case GLFW_KEY_PAGE_UP: imgui_key = ImGuiKey_PageUp; break;
            case GLFW_KEY_PAGE_DOWN: imgui_key = ImGuiKey_PageDown; break;
            case GLFW_KEY_HOME: imgui_key = ImGuiKey_Home; break;
            case GLFW_KEY_END: imgui_key = ImGuiKey_End; break;
            case GLFW_KEY_INSERT: imgui_key = ImGuiKey_Insert; break;
            case GLFW_KEY_DELETE: imgui_key = ImGuiKey_Delete; break;
            case GLFW_KEY_BACKSPACE: imgui_key = ImGuiKey_Backspace; break;
            case GLFW_KEY_SPACE: imgui_key = ImGuiKey_Space; break;
            case GLFW_KEY_ENTER: imgui_key = ImGuiKey_Enter; break;
            case GLFW_KEY_ESCAPE: imgui_key = ImGuiKey_Escape; break;
            case GLFW_KEY_A: imgui_key = ImGuiKey_A; break;
            case GLFW_KEY_C: imgui_key = ImGuiKey_C; break;
            case GLFW_KEY_V: imgui_key = ImGuiKey_V; break;
            case GLFW_KEY_X: imgui_key = ImGuiKey_X; break;
            case GLFW_KEY_Y: imgui_key = ImGuiKey_Y; break;
            case GLFW_KEY_Z: imgui_key = ImGuiKey_Z; break;
            default: break;
        }
        if (imgui_key != ImGuiKey_None)
        {
            io.AddKeyEvent(imgui_key, (action == GLFW_PRESS || action == GLFW_REPEAT));
        }
    }

    // If ImGui wants keyboard, don't forward to the app
    if (!g_ImGuiGlfwBackendEnabled)
    {
        io.AddInputCharacter(c);
    }
        return;

    auto* samplePtr = static_cast<SampleBase*>(glfwGetWindowUserPointer(w));
    if (!samplePtr)
        return;

    auto& macCtrl = reinterpret_cast<Diligent::InputControllerMacOS&>(samplePtr->GetInputController());

    if (action == GLFW_PRESS || action == GLFW_REPEAT)
        macCtrl.OnKeyPressed(key);
    else if (action == GLFW_RELEASE)
        macCtrl.OnKeyReleased(key);

    // Forward modifier state as flags change
    macCtrl.OnFlagsChanged((mods & GLFW_MOD_SHIFT) != 0,
                           (mods & GLFW_MOD_CONTROL) != 0,
                           (mods & GLFW_MOD_ALT) != 0);
}

static void GLFWMouseButtonCallbackMac(GLFWwindow* w, int button, int action, int mods)
{
    ImGuiIO& io = ImGui::GetIO();
    if (!g_ImGuiGlfwBackendEnabled)
    {
        if (button >= 0 && button < ImGuiMouseButton_COUNT)
            io.AddMouseButtonEvent(button, action == GLFW_PRESS);
    }
    if (io.WantCaptureMouse)
        return;

    auto* samplePtr = static_cast<SampleBase*>(glfwGetWindowUserPointer(w));
    if (!samplePtr)
        return;

    auto& macCtrl = reinterpret_cast<Diligent::InputControllerMacOS&>(samplePtr->GetInputController());

    if (button == GLFW_MOUSE_BUTTON_LEFT)
    {
        if (action == GLFW_PRESS)
            macCtrl.OnMouseButtonEvent(Diligent::InputControllerMacOS::MouseButtonEvent::LMB_Pressed);
        else if (action == GLFW_RELEASE)
            macCtrl.OnMouseButtonEvent(Diligent::InputControllerMacOS::MouseButtonEvent::LMB_Released);
    }
    else if (button == GLFW_MOUSE_BUTTON_RIGHT)
    {
        if (action == GLFW_PRESS)
            macCtrl.OnMouseButtonEvent(Diligent::InputControllerMacOS::MouseButtonEvent::RMB_Pressed);
        else if (action == GLFW_RELEASE)
            macCtrl.OnMouseButtonEvent(Diligent::InputControllerMacOS::MouseButtonEvent::RMB_Released);
    }
}

static void GLFWCursorPosCallbackMac(GLFWwindow* w, double xpos, double ypos)
{
    ImGuiIO& io = ImGui::GetIO();
    if (!g_ImGuiGlfwBackendEnabled)
    {
        io.AddMousePosEvent(static_cast<float>(xpos), static_cast<float>(ypos));
    }
    if (io.WantCaptureMouse)
        return;

    auto* samplePtr = static_cast<SampleBase*>(glfwGetWindowUserPointer(w));
    if (!samplePtr)
        return;

    auto& macCtrl = reinterpret_cast<Diligent::InputControllerMacOS&>(samplePtr->GetInputController());
    macCtrl.OnMouseMove(static_cast<int>(xpos), static_cast<int>(ypos));
}

static void GLFWScrollCallbackMac(GLFWwindow* w, double xoffset, double yoffset)
{
    ImGuiIO& io = ImGui::GetIO();
    if (!g_ImGuiGlfwBackendEnabled)
    {
        io.AddMouseWheelEvent(static_cast<float>(xoffset), static_cast<float>(yoffset));
    }
    if (io.WantCaptureMouse)
        return;

    auto* samplePtr = static_cast<SampleBase*>(glfwGetWindowUserPointer(w));
    if (!samplePtr)
        return;

    auto& macCtrl = reinterpret_cast<Diligent::InputControllerMacOS&>(samplePtr->GetInputController());
    macCtrl.OnMouseWheel(static_cast<float>(yoffset));
}
#endif

#if defined(__linux__)
// Include X11 headers needed for key mapping
#include <X11/keysym.h>

// Linux-specific GLFW callbacks that forward events into Diligent InputControllerLinux
static Diligent::InputKeys MapGLFWKeyToInputKey(int key)
{
    using namespace Diligent;
    switch (key)
    {
        case GLFW_KEY_LEFT_CONTROL:
        case GLFW_KEY_RIGHT_CONTROL:
            return InputKeys::ControlDown;
        case GLFW_KEY_LEFT_SHIFT:
        case GLFW_KEY_RIGHT_SHIFT:
            return InputKeys::ShiftDown;
        case GLFW_KEY_LEFT_ALT:
        case GLFW_KEY_RIGHT_ALT:
            return InputKeys::AltDown;
        case GLFW_KEY_LEFT:
        case GLFW_KEY_A:
            return InputKeys::MoveLeft;
        case GLFW_KEY_RIGHT:
        case GLFW_KEY_D:
            return InputKeys::MoveRight;
        case GLFW_KEY_UP:
        case GLFW_KEY_W:
            return InputKeys::MoveForward;
        case GLFW_KEY_DOWN:
        case GLFW_KEY_S:
            return InputKeys::MoveBackward;
        case GLFW_KEY_PAGE_UP:
        case GLFW_KEY_E:
            return InputKeys::MoveUp;
        case GLFW_KEY_PAGE_DOWN:
        case GLFW_KEY_Q:
            return InputKeys::MoveDown;
        case GLFW_KEY_HOME:
            return InputKeys::Reset;
        case GLFW_KEY_KP_ADD:
        case GLFW_KEY_EQUAL:
            return InputKeys::ZoomIn;
        case GLFW_KEY_KP_SUBTRACT:
        case GLFW_KEY_MINUS:
            return InputKeys::ZoomOut;
        default:
            return InputKeys::Unknown;
    }
}

// Helper class to access protected members of InputControllerBase
class InputControllerLinuxHelper : public Diligent::InputControllerBase
{
public:
    void SetKeyState(Diligent::InputKeys key, bool isPressed)
    {
        auto& keyState = m_Keys[static_cast<size_t>(key)];
        if (isPressed)
        {
            keyState &= ~INPUT_KEY_STATE_FLAG_KEY_WAS_DOWN;
            keyState |= INPUT_KEY_STATE_FLAG_KEY_IS_DOWN;
        }
        else
        {
            keyState &= ~INPUT_KEY_STATE_FLAG_KEY_IS_DOWN;
            keyState |= INPUT_KEY_STATE_FLAG_KEY_WAS_DOWN;
        }
    }

    void SetMousePosition(float x, float y)
    {
        m_MouseState.PosX = x;
        m_MouseState.PosY = y;
    }

    void SetMouseButton(Diligent::MouseState::BUTTON_FLAGS flag, bool pressed)
    {
        if (pressed)
            m_MouseState.ButtonFlags |= flag;
        else
            m_MouseState.ButtonFlags &= ~flag;
    }

    void AddWheelDelta(int delta)
    {
        m_MouseState.WheelDelta += delta;
    }
};

static void GLFWKeyCallbackLinux(GLFWwindow* w, int key, int scancode, int action, int mods)
{
    // Handle ImGui input using modern API
    ImGuiIO& io = ImGui::GetIO();
    if (!g_ImGuiGlfwBackendEnabled)
    {
        io.AddKeyEvent(ImGuiMod_Ctrl, (mods & GLFW_MOD_CONTROL) != 0);
        io.AddKeyEvent(ImGuiMod_Shift, (mods & GLFW_MOD_SHIFT) != 0);
        io.AddKeyEvent(ImGuiMod_Alt, (mods & GLFW_MOD_ALT) != 0);
        io.AddKeyEvent(ImGuiMod_Super, (mods & GLFW_MOD_SUPER) != 0);

        ImGuiKey imgui_key = ImGuiKey_None;
        switch (key)
        {
            case GLFW_KEY_TAB: imgui_key = ImGuiKey_Tab; break;
            case GLFW_KEY_LEFT: imgui_key = ImGuiKey_LeftArrow; break;
            case GLFW_KEY_RIGHT: imgui_key = ImGuiKey_RightArrow; break;
            case GLFW_KEY_UP: imgui_key = ImGuiKey_UpArrow; break;
            case GLFW_KEY_DOWN: imgui_key = ImGuiKey_DownArrow; break;
            case GLFW_KEY_PAGE_UP: imgui_key = ImGuiKey_PageUp; break;
            case GLFW_KEY_PAGE_DOWN: imgui_key = ImGuiKey_PageDown; break;
            case GLFW_KEY_HOME: imgui_key = ImGuiKey_Home; break;
            case GLFW_KEY_END: imgui_key = ImGuiKey_End; break;
            case GLFW_KEY_INSERT: imgui_key = ImGuiKey_Insert; break;
            case GLFW_KEY_DELETE: imgui_key = ImGuiKey_Delete; break;
            case GLFW_KEY_BACKSPACE: imgui_key = ImGuiKey_Backspace; break;
            case GLFW_KEY_SPACE: imgui_key = ImGuiKey_Space; break;
            case GLFW_KEY_ENTER: imgui_key = ImGuiKey_Enter; break;
            case GLFW_KEY_ESCAPE: imgui_key = ImGuiKey_Escape; break;
            case GLFW_KEY_A: imgui_key = ImGuiKey_A; break;
            case GLFW_KEY_C: imgui_key = ImGuiKey_C; break;
            case GLFW_KEY_V: imgui_key = ImGuiKey_V; break;
            case GLFW_KEY_X: imgui_key = ImGuiKey_X; break;
            case GLFW_KEY_Y: imgui_key = ImGuiKey_Y; break;
            case GLFW_KEY_Z: imgui_key = ImGuiKey_Z; break;
            default: break;
        }

        if (imgui_key != ImGuiKey_None)
        {
            io.AddKeyEvent(imgui_key, (action == GLFW_PRESS || action == GLFW_REPEAT));
        }
    }
    
    // If ImGui wants keyboard, don't forward to the app
    if (io.WantCaptureKeyboard)
        return;

    auto* samplePtr = static_cast<SampleBase*>(glfwGetWindowUserPointer(w));
    if (!samplePtr)
        return;

    auto inputKey = MapGLFWKeyToInputKey(key);
    if (inputKey == Diligent::InputKeys::Unknown)
        return;

    auto& linuxCtrl = reinterpret_cast<InputControllerLinuxHelper&>(samplePtr->GetInputController());
    bool isPressed = (action == GLFW_PRESS || action == GLFW_REPEAT);
    linuxCtrl.SetKeyState(inputKey, isPressed);
}

static void GLFWMouseButtonCallbackLinux(GLFWwindow* w, int button, int action, int mods)
{
    // Handle ImGui mouse input
    ImGuiIO& io = ImGui::GetIO();
    if (!g_ImGuiGlfwBackendEnabled)
    {
        if (button >= 0 && button < ImGuiMouseButton_COUNT)
            io.AddMouseButtonEvent(button, action == GLFW_PRESS);
    }
    
    if (io.WantCaptureMouse)
        return;

    auto* samplePtr = static_cast<SampleBase*>(glfwGetWindowUserPointer(w));
    if (!samplePtr)
        return;

    auto& linuxCtrl = reinterpret_cast<InputControllerLinuxHelper&>(samplePtr->GetInputController());

    if (button == GLFW_MOUSE_BUTTON_LEFT)
    {
        linuxCtrl.SetMouseButton(Diligent::MouseState::BUTTON_FLAG_LEFT, action == GLFW_PRESS);
    }
    else if (button == GLFW_MOUSE_BUTTON_RIGHT)
    {
        linuxCtrl.SetMouseButton(Diligent::MouseState::BUTTON_FLAG_RIGHT, action == GLFW_PRESS);
    }
    else if (button == GLFW_MOUSE_BUTTON_MIDDLE)
    {
        linuxCtrl.SetMouseButton(Diligent::MouseState::BUTTON_FLAG_MIDDLE, action == GLFW_PRESS);
    }
}

static void GLFWCursorPosCallbackLinux(GLFWwindow* w, double xpos, double ypos)
{
    // Update ImGui mouse position
    ImGuiIO& io = ImGui::GetIO();
    if (!g_ImGuiGlfwBackendEnabled)
    {
        io.AddMousePosEvent(static_cast<float>(xpos), static_cast<float>(ypos));
    }
    
    if (io.WantCaptureMouse)
        return;

    auto* samplePtr = static_cast<SampleBase*>(glfwGetWindowUserPointer(w));
    if (!samplePtr)
        return;

    auto& linuxCtrl = reinterpret_cast<InputControllerLinuxHelper&>(samplePtr->GetInputController());
    linuxCtrl.SetMousePosition(static_cast<float>(xpos), static_cast<float>(ypos));
}

static void GLFWScrollCallbackLinux(GLFWwindow* w, double xoffset, double yoffset)
{
    // Handle ImGui scroll input
    ImGuiIO& io = ImGui::GetIO();
    if (!g_ImGuiGlfwBackendEnabled)
    {
        io.AddMouseWheelEvent(static_cast<float>(xoffset), static_cast<float>(yoffset));
    }
    
    if (io.WantCaptureMouse)
        return;

    auto* samplePtr = static_cast<SampleBase*>(glfwGetWindowUserPointer(w));
    if (!samplePtr)
        return;

    auto& linuxCtrl = reinterpret_cast<InputControllerLinuxHelper&>(samplePtr->GetInputController());
    linuxCtrl.AddWheelDelta(static_cast<int>(yoffset));
}
#endif



// Function to initialize Diligent Engine with the specified backend
bool InitializeDiligentEngine(
    GLFWwindow* window,
    RefCntAutoPtr<IEngineFactory>& factory,
    RefCntAutoPtr<IRenderDevice>& device,
    RefCntAutoPtr<IDeviceContext>& immediateContext,
    RefCntAutoPtr<ISwapChain>& swapChain,
    IDeviceContext* ppContexts[1])
{
    SwapChainDesc swapChainDesc{};
    swapChainDesc.Width  = 800;
    swapChainDesc.Height = 600;
    swapChainDesc.ColorBufferFormat = TEX_FORMAT_RGBA8_UNORM;
    swapChainDesc.DepthBufferFormat = TEX_FORMAT_D32_FLOAT;


    NativeWindowWrapper nativeWindow = GetNativeWindow(window);

#if defined(_WIN32)
    // Try D3D12
#if D3D12_SUPPORTED
    if (auto* pFactoryD3D12 = LoadAndGetEngineFactoryD3D12())
    {
        RefCntAutoPtr<IEngineFactoryD3D12> factoryD3D12(pFactoryD3D12);
        EngineD3D12CreateInfo engineCI{};
        factoryD3D12->CreateDeviceAndContextsD3D12(engineCI, &device, ppContexts);
        if (device)
        {
            immediateContext = ppContexts[0];
            factoryD3D12->CreateSwapChainD3D12(device, immediateContext, swapChainDesc, FullScreenModeDesc{}, nativeWindow.win32, &swapChain);
            if (swapChain) { factory = factoryD3D12; std::cout << "Render backend: D3D12\n"; return true; }
        }
    }
#endif

    // Try D3D11
#if D3D11_SUPPORTED
    if (auto* pFactoryD3D11 = LoadAndGetEngineFactoryD3D11())
    {
        RefCntAutoPtr<IEngineFactoryD3D11> factoryD3D11(pFactoryD3D11);
        EngineD3D11CreateInfo engineCI{};
        factoryD3D11->CreateDeviceAndContextsD3D11(engineCI, &device, ppContexts);
        if (device)
        {
            immediateContext = ppContexts[0];
            factoryD3D11->CreateSwapChainD3D11(device, immediateContext, swapChainDesc, FullScreenModeDesc{}, nativeWindow.win32, &swapChain);
            if (swapChain) { factory = factoryD3D11; std::cout << "Render backend: D3D11\n"; return true; }
        }
    }
#endif
#endif // _WIN32

#if defined(__APPLE__) && METAL_SUPPORTED
    if (auto* pFactoryMtl = LoadAndGetEngineFactoryMtl())
    {
        RefCntAutoPtr<IEngineFactoryMtl> factoryMtl(pFactoryMtl); 
        EngineMtlCreateInfo engineCI{};
        factoryMtl->CreateDeviceAndContextsMtl(engineCI, &device, ppContexts);
        if (device)
        {
            immediateContext = ppContexts[0];
            factoryMtl->CreateSwapChainMtl(device, immediateContext, swapChainDesc, nativeWindow.mac, &swapChain);
            if (swapChain) { factory = factoryMtl; std::cout << "Render backend: Metal\n"; return true; }
        }
    }
#endif

#if VULKAN_SUPPORTED
    if (auto* pFactoryVk = LoadAndGetEngineFactoryVk())
    {
        RefCntAutoPtr<IEngineFactoryVk> factoryVk(pFactoryVk); 
        EngineVkCreateInfo engineCI{};
        factoryVk->CreateDeviceAndContextsVk(engineCI, &device, ppContexts);
        if (device)
        {
            immediateContext = ppContexts[0];
#if defined(_WIN32)
            factoryVk->CreateSwapChainVk(device, immediateContext, swapChainDesc, nativeWindow.win32, &swapChain);
#elif defined(__APPLE__)
            factoryVk->CreateSwapChainVk(device, immediateContext, swapChainDesc, nativeWindow.mac, &swapChain);
#elif defined(__linux__)
            factoryVk->CreateSwapChainVk(device, immediateContext, swapChainDesc, nativeWindow.x11, &swapChain);
#endif
            if (swapChain) { factory = factoryVk; std::cout << "Render backend: Vulkan\n"; return true; }
        }
    }
#endif

#if GL_SUPPORTED || GLES_SUPPORTED
    if (auto* pFactoryGL = LoadAndGetEngineFactoryOpenGL())
    {
        RefCntAutoPtr<IEngineFactoryOpenGL> factoryGL(pFactoryGL);
        EngineGLCreateInfo engineCI{};
#if defined(_WIN32)
        engineCI.Window.hWnd = nativeWindow.win32.hWnd;
#elif defined(__APPLE__)
        engineCI.Window.pNSView = nativeWindow.mac.pNSView;
#elif defined(__linux__)
        engineCI.Window.WindowId = nativeWindow.x11.WindowId;
        engineCI.Window.pDisplay = nativeWindow.x11.pDisplay;
#endif
        factoryGL->CreateDeviceAndSwapChainGL(engineCI, &device, &immediateContext, swapChainDesc, &swapChain);
        if (device && swapChain) { ppContexts[0] = immediateContext; factory = factoryGL; std::cout << "Render backend: OpenGL\n"; return true; }
    }
#endif

    return false;
}

int main()
{
    // -------------------
    // Init GLFW
    // -------------------
    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(800, 600, "C6GE 2026.1", nullptr, nullptr);
    if (!window)
    {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    #ifdef __APPLE__
    NSWindow *ns_window = glfwGetCocoaWindow(window);
    NSView *view = [ns_window contentView];
    [view setWantsLayer:YES];
    [view setLayer:[CAMetalLayer layer]];
    #endif

    Diligent::C6GERender::IsRuntime = false;

    // -------------------
    // Create Diligent Engine
    // -------------------
    RefCntAutoPtr<IEngineFactory> factory;
    RefCntAutoPtr<IRenderDevice> device;
    RefCntAutoPtr<IDeviceContext> immediateContext;
    RefCntAutoPtr<ISwapChain> swapChain;
    IDeviceContext* ppContexts[1] = {nullptr};

    if (!InitializeDiligentEngine(window, factory, device, immediateContext, swapChain, ppContexts))
    {
        std::cerr << "Failed to initialize any rendering backend" << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    // -------------------
    // Initialize ImGui
    // -------------------
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable keyboard navigation
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     // Enable docking
    ImGui::StyleColorsDark();

    // --- DPI/UI scaling: Make UI bigger ---
    float xscale = 1.0f, yscale = 1.0f;
#if GLFW_VERSION_MAJOR > 3 || (GLFW_VERSION_MAJOR == 3 && GLFW_VERSION_MINOR >= 3)
    glfwGetWindowContentScale(window, &xscale, &yscale);
#endif
    float font_scale = xscale; // Use xscale for font and style scaling

    // Load a larger font (Roboto-Medium.ttf is available in src/Assets/)
    ImGuiIO& io_font = ImGui::GetIO();
    io_font.Fonts->Clear();
    
    // Try multiple possible paths for the font file
    const char* font_paths[] = {
        "Roboto-Medium.ttf",
        "Roboto-Medium.ttf", 
        "Roboto-Medium.ttf",
        "Roboto-Medium.ttf"
    };
    
    bool font_loaded = false;
    for (const char* font_path : font_paths) {
        ImFont* font1 = io_font.Fonts->AddFontFromFileTTF(font_path, 10.0f * font_scale);
        ImFont* font2 = io_font.Fonts->AddFontFromFileTTF(font_path, 18.0f * font_scale);
        if (font1 != nullptr && font2 != nullptr) {
            font_loaded = true;
            std::cout << "Successfully loaded font from: " << font_path << std::endl;
            break;
        }
    }
    
    if (!font_loaded) {
        std::cout << "Warning: Could not load Roboto-Medium.ttf, using default font" << std::endl;
        io_font.Fonts->AddFontDefault();
    }
    // Scale all style sizes
    ImGui::GetStyle().ScaleAllSizes(font_scale);
    // Rebuild font atlas

    // Initialize ImGui Diligent backend (DiligentEngine handles GLFW integration internally)
    std::unique_ptr<ImGuiImplDiligent> imGuiImpl;
    try
    {
        ImGuiDiligentCreateInfo imGuiCI(device, swapChain->GetDesc());
        imGuiImpl = std::make_unique<ImGuiImplDiligent>(imGuiCI);
    }
    catch (const std::exception& e)
    {
        std::cerr << "Failed to initialize ImGui Diligent backend: " << e.what() << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }
    if (!imGuiImpl)
    {
        std::cerr << "Failed to initialize ImGui Diligent backend" << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    g_ImGuiRenderer = imGuiImpl.get();
    const bool multiViewportSupported = InitializeDiligentViewportGlobals(device, immediateContext, swapChain, factory);
    if (!multiViewportSupported)
    {
        std::cout << "[C6GE] ImGui multi-viewport not available for current backend; windows remain docked." << std::endl;
    }

    // -------------------
    // Initialize sample
    // -------------------
    SampleBase* sample = CreateSample();
    SampleInitInfo initInfo;
    initInfo.pDevice = device;
    initInfo.pSwapChain = swapChain;
    initInfo.ppContexts = ppContexts;
    initInfo.NumImmediateCtx = 1;
    initInfo.pEngineFactory = factory;
    initInfo.pImGui = imGuiImpl.get(); // Use raw pointer for SampleInitInfo
    sample->Initialize(initInfo);
    std::cout << "Initialized, forcing WindowResize..." << std::endl;
    int fbW, fbH;
    glfwGetFramebufferSize(window, &fbW, &fbH);
    swapChain->Resize(fbW, fbH);
    sample->WindowResize(fbW, fbH);

    auto* renderSample = static_cast<Diligent::C6GERender*>(sample);
    renderSample->SetHostWindow(window);

    // Forward GLFW events to Diligent InputController. We set the sample as
    // the user pointer so callbacks can access the InputController instance.
    glfwSetWindowUserPointer(window, sample);
#if defined(_WIN32)
    // These callbacks are implemented above only for Windows and forward
    // events into the Win32-specific InputController implementation.
    glfwSetKeyCallback(window, GLFWKeyCallback);
    glfwSetCharCallback(window, GLFWCharCallback);
    glfwSetMouseButtonCallback(window, GLFWMouseButtonCallback);
    glfwSetCursorPosCallback(window, GLFWCursorPosCallback);
    glfwSetScrollCallback(window, GLFWScrollCallback);
#elif defined(__APPLE__)
    // Register macOS callbacks that forward into InputControllerMacOS
    glfwSetKeyCallback(window, GLFWKeyCallbackMac);
    glfwSetMouseButtonCallback(window, GLFWMouseButtonCallbackMac);
    glfwSetCursorPosCallback(window, GLFWCursorPosCallbackMac);
    glfwSetScrollCallback(window, GLFWScrollCallbackMac);
    glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_TRUE);
#elif defined(__linux__)
    // Register Linux callbacks that forward into InputControllerLinux
    glfwSetKeyCallback(window, GLFWKeyCallbackLinux);
    glfwSetCharCallback(window, GLFWCharCallbackLinux);
    glfwSetMouseButtonCallback(window, GLFWMouseButtonCallbackLinux);
    glfwSetCursorPosCallback(window, GLFWCursorPosCallbackLinux);
    glfwSetScrollCallback(window, GLFWScrollCallbackLinux);
#endif

    if (multiViewportSupported)
    {
        if (!g_ImGuiGlfwBackendEnabled)
        {
            ImGui_ImplGlfw_InitForOther(window, true);
            g_ImGuiGlfwBackendEnabled = true;
        }
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
        ConfigureImGuiMultiViewport(io);
    }

EditorTheme::SetupImGuiStyle();
    
    // -------------------
    // Main loop
    // -------------------
    bool enableVsync = false;
    double lastTime = glfwGetTime();
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        // Window size (logical, used for input)
        int windowW = 0, windowH = 0;
        glfwGetWindowSize(window, &windowW, &windowH);

        // Framebuffer size (physical pixels)
        int fbW = 0, fbH = 0;
        glfwGetFramebufferSize(window, &fbW, &fbH);

        // DPI scale
        float dpiScaleX = (float)fbW / (float)windowW;
        float dpiScaleY = (float)fbH / (float)windowH;

        // Resize swap chain if needed
        const auto& scDesc = swapChain->GetDesc();
        if (fbW != static_cast<int>(scDesc.Width) || fbH != static_cast<int>(scDesc.Height))
        {
            swapChain->Resize(fbW, fbH);
            sample->WindowResize(fbW, fbH);
        }

        // Compute elapsed time and start ImGui frame
        double currTime = glfwGetTime();
        double elapsed = currTime - lastTime;
        lastTime = currTime;
        
    if (g_ImGuiGlfwBackendEnabled)
        ImGui_ImplGlfw_NewFrame();

    // Set DisplaySize and scale for Retina/HiDPI
// Logical size = window coordinates (points)
io.DisplaySize = ImVec2((float)windowW, (float)windowH);

// Framebuffer scale = how many pixels per point
#if defined(__APPLE__) || defined(__linux__)
    io.DisplayFramebufferScale = ImVec2(dpiScaleX, dpiScaleY);
#else
    io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
#endif

// Tell backend the logical window size (matches DisplaySize)
imGuiImpl->NewFrame(windowW, windowH, scDesc.PreTransform);

    if (renderSample != nullptr)
        renderSample->UpdateViewportUI();

    // Build UI, pass elapsed time so camera receives proper dt
    sample->Update(currTime, elapsed, true);

        // First render the scene to the framebuffer
        sample->Render();

        // Then render ImGui to the swap chain
        ITextureView* pRTV = swapChain->GetCurrentBackBufferRTV();
        ITextureView* pDSV = swapChain->GetDepthBufferDSV();
        
        // Clear the swap chain back buffer (for ImGui background)
        Diligent::float4 ClearColor = {0.1f, 0.1f, 0.1f, 1.0f};
        immediateContext->SetRenderTargets(1, &pRTV, pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        immediateContext->ClearRenderTarget(pRTV, ClearColor.Data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        immediateContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        
        imGuiImpl->Render(immediateContext);

        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();

            // Rebind main render target for subsequent operations
            pRTV = swapChain->GetCurrentBackBufferRTV();
            pDSV = swapChain->GetDepthBufferDSV();
            ITextureView* rtvs[] = {pRTV};
            immediateContext->SetRenderTargets(1, rtvs, pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        }

        swapChain->Present(enableVsync ? 1 : 0);
    }

    // -------------------
    // Cleanup
    // -------------------
    if (g_ImGuiGlfwBackendEnabled)
    {
        ImGui_ImplGlfw_Shutdown();
        g_ImGuiGlfwBackendEnabled = false;
    }
    g_ImGuiRenderer = nullptr;
    g_RenderDevice.Release();
    g_ImmediateContext.Release();
    g_MainSwapChain.Release();
#if defined(_WIN32)
#    if D3D11_SUPPORTED
    g_FactoryD3D11.Release();
#    endif
#    if D3D12_SUPPORTED
    g_FactoryD3D12.Release();
#    endif
#endif
    // Tear down ImGui backend first to ensure it no longer references engine views
    imGuiImpl.reset();
    // Then destroy the sample (releases textures/views)
    delete sample;
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
