#define NOMINMAX

#include "../Window/Window.h"

#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <bx/math.h>
#include <bx/timer.h>
#include <iostream>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_bgfx.h>
#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <bx/math.h>
#include <cstdint>

// Include shader binary data
#include <glsl/vs_cubes.sc.bin.h>
#include <essl/vs_cubes.sc.bin.h>
#include <glsl/fs_cubes.sc.bin.h>
#include <essl/fs_cubes.sc.bin.h>
#include <spirv/vs_cubes.sc.bin.h>
#include <spirv/fs_cubes.sc.bin.h>

#if defined(_WIN32)
#include "dx11/vs_cubes.sc.bin.h"
#include "dx11/fs_cubes.sc.bin.h"
#endif
#if __APPLE__
#include "../mtl/vs_cubes.sc.bin.h"
#include "../mtl/fs_cubes.sc.bin.h"
#endif




// Vertex structure and layout
struct PosColorVertex {
    float m_x, m_y, m_z;
    uint32_t m_abgr;
    static bgfx::VertexLayout ms_layout;
    static void init() {
        ms_layout
            .begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
            .end();
    }
};
bgfx::VertexLayout PosColorVertex::ms_layout;

static PosColorVertex s_cubeVertices[] = {
    {-1.0f,  1.0f,  1.0f, 0xff000000 },
    { 1.0f,  1.0f,  1.0f, 0xff0000ff },
    {-1.0f, -1.0f,  1.0f, 0xff00ff00 },
    { 1.0f, -1.0f,  1.0f, 0xff00ffff },
    {-1.0f,  1.0f, -1.0f, 0xffff0000 },
    { 1.0f,  1.0f, -1.0f, 0xffff00ff },
    {-1.0f, -1.0f, -1.0f, 0xffffff00 },
    { 1.0f, -1.0f, -1.0f, 0xffffffff },
};

static const uint16_t s_cubeTriList[] = {
    0, 1, 2, 1, 3, 2, 4, 6, 5, 5, 6, 7, 0, 2, 4, 4, 2, 6, 1, 5, 3, 5, 7, 3, 0, 4, 1, 4, 5, 1, 2, 3, 6, 6, 3, 7,
};
static const uint16_t s_cubeTriStrip[] = {
    0, 1, 2, 3, 7, 1, 5, 0, 4, 2, 6, 7, 4, 5,
};
static const uint16_t s_cubeLineList[] = {
    0, 1, 0, 2, 0, 4, 1, 3, 1, 5, 2, 3, 2, 6, 3, 7, 4, 5, 4, 6, 5, 7, 6, 7,
};
static const uint16_t s_cubeLineStrip[] = {
    0, 2, 3, 1, 5, 7, 6, 4, 0, 2, 6, 4, 5, 7, 3, 1, 0,
};
static const uint16_t s_cubePoints[] = {
    0, 1, 2, 3, 4, 5, 6, 7
};
static const char* s_ptNames[] = {
    "Triangle List", "Triangle Strip", "Lines", "Line Strip", "Points",
};
static const uint64_t s_ptState[] = {
    UINT64_C(0), BGFX_STATE_PT_TRISTRIP, BGFX_STATE_PT_LINES, BGFX_STATE_PT_LINESTRIP, BGFX_STATE_PT_POINTS,
};

namespace C6GE {
bool EngineRun() {
    C6GE::Window window;

    if (!window.Init()) {
        std::cout << "Failed to initialize window" << std::endl;
        return false;
    }

    if (!window.CreateGLFWWindow("C6GE Cubes", 1280, 720)) {
        std::cout << "Failed to create GLFW window" << std::endl;
        return false;
    }
    
    GLFWwindow* glfwWindow = window.GetWindow();
    
    // Process events to ensure the window is properly created
    window.HandleWindowEvents();
    
    // Ensure window is visible and ready
    window.ShowWindow();
    window.FocusWindow();
    
    // Set up window resize callback
    window.SetFramebufferSizeCallback([](GLFWwindow* window, int width, int height) {
        bgfx::reset(width, height, BGFX_RESET_VSYNC);
        bgfx::setViewRect(0, 0, 0, uint16_t(width), uint16_t(height));
        bgfx::setViewClear(0, BGFX_CLEAR_COLOR|BGFX_CLEAR_DEPTH, 0x303030ff, 1.0f, 0);
    });
    
    // Single threaded mode - this is crucial!
    bgfx::renderFrame();
    bgfx::PlatformData pd{};
#if BX_PLATFORM_WINDOWS
    pd.nwh = window.GetWin32Window();
#elif BX_PLATFORM_OSX
    pd.nwh = window.GetCocoaWindow();
#elif BX_PLATFORM_LINUX
    pd.nwh = window.GetX11Window();
    pd.ndt = window.GetX11Display();
#endif

    bgfx::Init init;
#if BX_PLATFORM_WINDOWS
    // Try DX12 first, fall back to DX11 if it fails
    init.type = bgfx::RendererType::Direct3D12;
    init.resolution.width = 1280;
    init.resolution.height = 720;
    init.resolution.reset = BGFX_RESET_VSYNC;
    init.platformData = pd;
    
    if (!bgfx::init(init)) {
        std::cout << "DX12 failed, trying DX11..." << std::endl;
        init.type = bgfx::RendererType::Direct3D11;
        if (!bgfx::init(init)) {
            std::cout << "Failed to initialize bgfx with DX11" << std::endl;
            return false;
        }
    }
#else
    init.type = bgfx::RendererType::Count; // Auto-detect renderer on other platforms
    init.resolution.width = 1280;
    init.resolution.height = 720;
    init.resolution.reset = BGFX_RESET_VSYNC;
    init.platformData = pd;
    
    if (!bgfx::init(init)) { 
        std::cout << "Failed to initialize bgfx" << std::endl; 
        return false; 
    }
#endif

    PosColorVertex::init();
    bgfx::VertexBufferHandle vbh = bgfx::createVertexBuffer(bgfx::makeRef(s_cubeVertices, sizeof(s_cubeVertices)), PosColorVertex::ms_layout);
    bgfx::IndexBufferHandle ibh[5];
    ibh[0] = bgfx::createIndexBuffer(bgfx::makeRef(s_cubeTriList, sizeof(s_cubeTriList)));
    ibh[1] = bgfx::createIndexBuffer(bgfx::makeRef(s_cubeTriStrip, sizeof(s_cubeTriStrip)));
    ibh[2] = bgfx::createIndexBuffer(bgfx::makeRef(s_cubeLineList, sizeof(s_cubeLineList)));
    ibh[3] = bgfx::createIndexBuffer(bgfx::makeRef(s_cubeLineStrip, sizeof(s_cubeLineStrip)));
    ibh[4] = bgfx::createIndexBuffer(bgfx::makeRef(s_cubePoints, sizeof(s_cubePoints)));


    // Select correct shader binary for renderer
    const uint8_t* vs_bin = nullptr;
    size_t vs_bin_size = 0;
    const uint8_t* fs_bin = nullptr;
    size_t fs_bin_size = 0;

    switch (bgfx::getRendererType()) {
#if defined(_WIN32)
        case bgfx::RendererType::Direct3D12:
            vs_bin = vs_cubes_dx11;
            vs_bin_size = sizeof(vs_cubes_dx11);
            fs_bin = fs_cubes_dx11;
            fs_bin_size = sizeof(fs_cubes_dx11);
            break;
        case bgfx::RendererType::Direct3D11:
            vs_bin = vs_cubes_dx11;
            vs_bin_size = sizeof(vs_cubes_dx11);
            fs_bin = fs_cubes_dx11;
            fs_bin_size = sizeof(fs_cubes_dx11);
            break;
#endif
        case bgfx::RendererType::OpenGL:
            vs_bin = vs_cubes_glsl;
            vs_bin_size = sizeof(vs_cubes_glsl);
            fs_bin = fs_cubes_glsl;
            fs_bin_size = sizeof(fs_cubes_glsl);
            break;
        case bgfx::RendererType::OpenGLES:
            vs_bin = vs_cubes_essl;
            vs_bin_size = sizeof(vs_cubes_essl);
            fs_bin = fs_cubes_essl;
            fs_bin_size = sizeof(fs_cubes_essl);
        case bgfx::RendererType::Vulkan:
            vs_bin = vs_cubes_spv;
            vs_bin_size = sizeof(vs_cubes_spv);
            fs_bin = vs_cubes_spv;
            fs_bin_size = sizeof(vs_cubes_spv);
            break;
#if defined(__APPLE__)
        case bgfx::RendererType::Metal:
            vs_bin = vs_cubes_mtl;
            vs_bin_size = sizeof(vs_cubes_mtl);
            fs_bin = fs_cubes_mtl;
            fs_bin_size = sizeof(fs_cubes_mtl);
            break;
#endif
        default:
            std::cout << "Unsupported renderer for shader loading!" << std::endl;
            return false;
    }

    bgfx::ShaderHandle vsh = bgfx::createShader(bgfx::makeRef(vs_bin, vs_bin_size));
    bgfx::ShaderHandle fsh = bgfx::createShader(bgfx::makeRef(fs_bin, fs_bin_size));
    bgfx::ProgramHandle program = bgfx::createProgram(vsh, fsh, true);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(glfwWindow, true);
    ImGui_Implbgfx_Init(1);

    int m_pt = 0;
    bool m_r = true, m_g = true, m_b = true, m_a = true;
    int width = 1280, height = 720;
    int frameCount = 0;
    int64_t timeOffset = bx::getHPCounter();

    while (!window.WindowShouldClose()) {
        window.HandleWindowEvents();
        
        // Handle window events more frequently
        if (glfwGetKey(glfwWindow, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            window.SetWindowShouldClose(true);
        }
        
        // Check if window needs refresh
        if (window.GetWindowAttrib(GLFW_ICONIFIED)) {
            window.HandleWindowEvents();
            continue; // Skip rendering when minimized
        }

        float time = float((bx::getHPCounter() - timeOffset) / double(bx::getHPFrequency()));
        const bx::Vec3 at  = { 0.0f, 0.0f,   0.0f };
        const bx::Vec3 eye = { 0.0f, 0.0f, -35.0f };
        float view[16]; bx::mtxLookAt(view, eye, at);
        float proj[16]; bx::mtxProj(proj, 60.0f, float(width)/float(height), 0.1f, 100.0f, bgfx::getCaps()->homogeneousDepth);
        bgfx::setViewTransform(0, view, proj);
        bgfx::setViewRect(0, 0, 0, uint16_t(width), uint16_t(height));
        bgfx::setViewClear(0, BGFX_CLEAR_COLOR|BGFX_CLEAR_DEPTH, 0x303030ff, 1.0f, 0);
        bgfx::touch(0);

        uint64_t state = 0
            | (m_r ? BGFX_STATE_WRITE_R : 0)
            | (m_g ? BGFX_STATE_WRITE_G : 0)
            | (m_b ? BGFX_STATE_WRITE_B : 0)
            | (m_a ? BGFX_STATE_WRITE_A : 0)
            | BGFX_STATE_WRITE_Z
            | BGFX_STATE_DEPTH_TEST_LESS
            | BGFX_STATE_CULL_CW
            | BGFX_STATE_MSAA
            | s_ptState[m_pt];

        for (uint32_t yy = 0; yy < 11; ++yy) {
            for (uint32_t xx = 0; xx < 11; ++xx) {
                float mtx[16];
                bx::mtxRotateXY(mtx, time + xx*0.21f, time + yy*0.37f);
                mtx[12] = -15.0f + float(xx)*3.0f;
                mtx[13] = -15.0f + float(yy)*3.0f;
                mtx[14] = 0.0f;
                bgfx::setTransform(mtx);
                bgfx::setVertexBuffer(0, vbh);
                bgfx::setIndexBuffer(ibh[m_pt]);
                bgfx::setState(state);
                bgfx::submit(0, program);
            }
        }

        ImGui_ImplGlfw_NewFrame();
        ImGui_Implbgfx_NewFrame();
        ImGui::NewFrame();
        ImGui::Begin("Settings");
        ImGui::Checkbox("Write R", &m_r);
        ImGui::Checkbox("Write G", &m_g);
        ImGui::Checkbox("Write B", &m_b);
        ImGui::Checkbox("Write A", &m_a);
        ImGui::Text("Primitive topology:");
        ImGui::Combo("##topology", &m_pt, s_ptNames, IM_ARRAYSIZE(s_ptNames));
        ImGui::End();
        ImGui::Render();
        ImGui_Implbgfx_RenderDrawLists(ImGui::GetDrawData());
        bgfx::frame();
    }

    ImGui_Implbgfx_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    for (int i = 0; i < 5; ++i) bgfx::destroy(ibh[i]);
    bgfx::destroy(vbh);
    bgfx::destroy(program);
    bgfx::shutdown();
    window.DestroyWindow();
    return true;
}
}
