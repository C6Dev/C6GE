
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
#include "MapHelper.hpp"
#include "GraphicsUtilities.h"
#include "ShaderMacroHelper.hpp"
#include "TextureUtilities.h"
#include "ColorConversion.h"
#include "../../Common/src/TexturedCube.hpp"
#include "imgui.h"
#include "ImGuiUtils.hpp"
#include "backends/imgui_impl_glfw.h"
#include "ImGuiImplDiligent.hpp"
#include "DiligentTools/ThirdParty/imGuIZMO.quat/imGuIZMO.h"
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


using namespace Diligent;



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
    // Forward to ImGui backend first
    ImGui_ImplGlfw_KeyCallback(w, key, scancode, action, mods);

    // If ImGui wants keyboard, don't forward to the app
    ImGuiIO& io = ImGui::GetIO();
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
    // Forward to ImGui backend first
    ImGui_ImplGlfw_MouseButtonCallback(w, button, action, mods);

    ImGuiIO& io = ImGui::GetIO();
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
    // Forward to ImGui
    ImGui_ImplGlfw_CursorPosCallback(w, xpos, ypos);
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse)
        return;

    auto* samplePtr = static_cast<SampleBase*>(glfwGetWindowUserPointer(w));
    if (!samplePtr)
        return;
    // Mouse position/state is handled by the platform input controller; no action needed here.
}

static void GLFWScrollCallback(GLFWwindow* w, double xoffset, double yoffset)
{
    // Forward to ImGui
    ImGui_ImplGlfw_ScrollCallback(w, xoffset, yoffset);
    ImGuiIO& io = ImGui::GetIO();
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
#endif

#if defined(__APPLE__)
// macOS-specific GLFW callbacks that forward events into Diligent InputControllerMacOS
static void GLFWKeyCallbackMac(GLFWwindow* w, int key, int scancode, int action, int mods)
{
    // Forward to ImGui backend first
    ImGui_ImplGlfw_KeyCallback(w, key, scancode, action, mods);

    // If ImGui wants keyboard, don't forward to the app
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureKeyboard)
        return;

    auto* samplePtr = static_cast<SampleBase*>(glfwGetWindowUserPointer(w));
    if (!samplePtr)
        return;

    auto& macCtrl = reinterpret_cast<Diligent::InputController&>(samplePtr->GetInputController());

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
    // Forward to ImGui backend first
    ImGui_ImplGlfw_MouseButtonCallback(w, button, action, mods);

    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse)
        return;

    auto* samplePtr = static_cast<SampleBase*>(glfwGetWindowUserPointer(w));
    if (!samplePtr)
        return;

    auto& macCtrl = reinterpret_cast<Diligent::InputController&>(samplePtr->GetInputController());

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
    // Forward to ImGui
    ImGui_ImplGlfw_CursorPosCallback(w, xpos, ypos);
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse)
        return;

    auto* samplePtr = static_cast<SampleBase*>(glfwGetWindowUserPointer(w));
    if (!samplePtr)
        return;

    auto& macCtrl = reinterpret_cast<Diligent::InputController&>(samplePtr->GetInputController());
    macCtrl.OnMouseMove(static_cast<int>(xpos), static_cast<int>(ypos));
}

static void GLFWScrollCallbackMac(GLFWwindow* w, double xoffset, double yoffset)
{
    // Forward to ImGui
    ImGui_ImplGlfw_ScrollCallback(w, xoffset, yoffset);
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse)
        return;

    auto* samplePtr = static_cast<SampleBase*>(glfwGetWindowUserPointer(w));
    if (!samplePtr)
        return;

    auto& macCtrl = reinterpret_cast<Diligent::InputControllerMacOS&>(samplePtr->GetInputController());
    macCtrl.OnMouseWheel(static_cast<float>(yoffset));
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


    // Helper to get native window for current platform
    struct NativeWindow
{
#if defined(_WIN32)
        Win32NativeWindow win32{};
#elif defined(__APPLE__)
        MacOSNativeWindow mac{};
#elif defined(__linux__)
        LinuxNativeWindow x11{}; // Renamed to avoid potential macro conflict with 'linux'
#endif
    };

    auto GetNativeWindow = [&](GLFWwindow* w) -> NativeWindow {
        NativeWindow nw{};
#if defined(_WIN32)
        nw.win32.hWnd = glfwGetWin32Window(w);
#elif defined(__APPLE__)
        nw.mac.pNSView = [glfwGetCocoaWindow(w) contentView];
#elif defined(__linux__)
        nw.x11.WindowId = glfwGetX11Window(w);
        nw.x11.pDisplay = glfwGetX11Display();
#endif
        return nw;
    };

    NativeWindow nativeWindow = GetNativeWindow(window);

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
    GLFWwindow* window = glfwCreateWindow(800, 600, "C6GE - TextureArray with ImGui", nullptr, nullptr);
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

    Diligent::ShadowsSample::IsRuntime = true;

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
    ImGui::StyleColorsDark();

    // Initialize ImGui GLFW backend
    if (!ImGui_ImplGlfw_InitForOther(window, true))
    {
        std::cerr << "Failed to initialize ImGui GLFW backend" << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    // Initialize ImGui Diligent backend
    std::unique_ptr<ImGuiImplDiligent> imGuiImpl;
    try
    {
        ImGuiDiligentCreateInfo imGuiCI(device, swapChain->GetDesc());
        imGuiImpl = std::make_unique<ImGuiImplDiligent>(imGuiCI);
    }
    catch (const std::exception& e)
    {
        std::cerr << "Failed to initialize ImGui Diligent backend: " << e.what() << std::endl;
        ImGui_ImplGlfw_Shutdown();
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }
    if (!imGuiImpl)
    {
        std::cerr << "Failed to initialize ImGui Diligent backend" << std::endl;
        ImGui_ImplGlfw_Shutdown();
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
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

    // Forward GLFW events to Diligent InputController (Windows). We set the sample as
    // the user pointer so callbacks can access the InputController instance.
    glfwSetWindowUserPointer(window, sample);
#if defined(_WIN32)
    // These callbacks are implemented above only for Windows and forward
    // events into the Win32-specific InputController implementation.
    glfwSetKeyCallback(window, GLFWKeyCallback);
    glfwSetMouseButtonCallback(window, GLFWMouseButtonCallback);
    glfwSetCursorPosCallback(window, GLFWCursorPosCallback);
    glfwSetScrollCallback(window, GLFWScrollCallback);
#elif defined(__APPLE__)
    // Register macOS callbacks that forward into InputControllerMacOS
    glfwSetKeyCallback(window, GLFWKeyCallbackMac);
    glfwSetMouseButtonCallback(window, GLFWMouseButtonCallbackMac);
    glfwSetCursorPosCallback(window, GLFWCursorPosCallbackMac);
    glfwSetScrollCallback(window, GLFWScrollCallbackMac);
#endif

    
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
        ImGui_ImplGlfw_NewFrame();
        
        // Set DisplaySize and scale for Retina
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2((float)fbW, (float)fbH);
        io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f); // keep 1.0 because we're already using pixels

        imGuiImpl->NewFrame(fbW, fbH, scDesc.PreTransform);

    // Build UI, pass elapsed time so camera receives proper dt
    sample->Update(currTime, elapsed, true);

        

        // Render
        ImGui::Render();
        sample->Render();

        ITextureView* pRTV = swapChain->GetCurrentBackBufferRTV();
        ITextureView* pDSV = swapChain->GetDepthBufferDSV();
        immediateContext->SetRenderTargets(1, &pRTV, pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        imGuiImpl->Render(immediateContext);

        swapChain->Present(enableVsync ? 1 : 0);
    }

    // -------------------
    // Cleanup
    // -------------------
    delete sample;
    ImGui_ImplGlfw_Shutdown();
    imGuiImpl.reset();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
