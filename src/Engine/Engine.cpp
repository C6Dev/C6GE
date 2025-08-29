#define NOMINMAX

#include "../Window/Window.h"
#include "../Render/Render.h"

#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <bgfx/embedded_shader.h>
#include <bx/math.h>
#include <bx/timer.h>
#include <iostream>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_bgfx.h>
#include <bx/math.h>
#include <cstdint>

// Include shader binary data
#include <glsl/vs_raymarching.sc.bin.h>
#include <essl/vs_raymarching.sc.bin.h>
#include <glsl/fs_raymarching.sc.bin.h>
#include <essl/fs_raymarching.sc.bin.h>
#include <spirv/vs_raymarching.sc.bin.h>
#include <spirv/fs_raymarching.sc.bin.h>

#if defined(_WIN32)
#include "dx11/vs_raymarching.sc.bin.h"
#include "dx11/fs_raymarching.sc.bin.h"
#endif
#if __APPLE__
#include "../mtl/vs_raymarching.sc.bin.h"
#include "../mtl/fs_raymarching.sc.bin.h"
#endif


struct PosColorTexCoord0Vertex
{
	float m_x;
	float m_y;
	float m_z;
	uint32_t m_abgr;
	float m_u;
	float m_v;

	static void init()
	{
		ms_layout
			.begin()
			.add(bgfx::Attrib::Position,  3, bgfx::AttribType::Float)
			.add(bgfx::Attrib::Color0,    4, bgfx::AttribType::Uint8, true)
			.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
			.end();
	}

	static bgfx::VertexLayout ms_layout;
};

bgfx::VertexLayout PosColorTexCoord0Vertex::ms_layout;

void renderScreenSpaceQuad(uint8_t _view, bgfx::ProgramHandle _program, float _x, float _y, float _width, float _height)
{
	bgfx::TransientVertexBuffer tvb;
	bgfx::TransientIndexBuffer tib;

	if (bgfx::allocTransientBuffers(&tvb, PosColorTexCoord0Vertex::ms_layout, 4, &tib, 6) )
	{
		PosColorTexCoord0Vertex* vertex = (PosColorTexCoord0Vertex*)tvb.data;

		float zz = 0.0f;

		const float minx = _x;
		const float maxx = _x + _width;
		const float miny = _y;
		const float maxy = _y + _height;

		float minu = -1.0f;
		float minv = -1.0f;
		float maxu =  1.0f;
		float maxv =  1.0f;

		vertex[0].m_x = minx;
		vertex[0].m_y = miny;
		vertex[0].m_z = zz;
		vertex[0].m_abgr = 0xff0000ff;
		vertex[0].m_u = minu;
		vertex[0].m_v = minv;

		vertex[1].m_x = maxx;
		vertex[1].m_y = miny;
		vertex[1].m_z = zz;
		vertex[1].m_abgr = 0xff00ff00;
		vertex[1].m_u = maxu;
		vertex[1].m_v = minv;

		vertex[2].m_x = maxx;
		vertex[2].m_y = maxy;
		vertex[2].m_z = zz;
		vertex[2].m_abgr = 0xffff0000;
		vertex[2].m_u = maxu;
		vertex[2].m_v = maxv;

		vertex[3].m_x = minx;
		vertex[3].m_y = maxy;
		vertex[3].m_z = zz;
		vertex[3].m_abgr = 0xffffffff;
		vertex[3].m_u = minu;
		vertex[3].m_v = maxv;

		uint16_t* indices = (uint16_t*)tib.data;

		indices[0] = 0;
		indices[1] = 2;
		indices[2] = 1;
		indices[3] = 0;
		indices[4] = 3;
		indices[5] = 2;

		bgfx::setState(BGFX_STATE_DEFAULT);
		bgfx::setIndexBuffer(&tib);
		bgfx::setVertexBuffer(0, &tvb);
		bgfx::submit(_view, _program);
	}
}

namespace C6GE {

// Embedded shaders - bgfx will automatically select the right format
static const bgfx::EmbeddedShader s_embeddedShaders[] =
{
    BGFX_EMBEDDED_SHADER(vs_raymarching),
    BGFX_EMBEDDED_SHADER(fs_raymarching),

    BGFX_EMBEDDED_SHADER_END()
};

bool EngineRun() {
    C6GE::Window window;
    C6GE::Render render;
    
    // Initialize window dimensions
    int width = 1280;
    int height = 720;
    
    // Initialize camera position
    bx::Vec3 eye = {0.0f, 0.0f, -2.5f};
    bx::Vec3 at = {0.0f, 0.0f, 0.0f};

    if (!window.Init()) {
        std::cout << "Failed to initialize window" << std::endl;
        return false;
    }

    if (!window.CreateGLFWWindow("C6GE Cubes", 1280, 720)) {
        std::cout << "Failed to create window" << std::endl;
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
        bgfx::setViewClear(0, BGFX_CLEAR_COLOR|BGFX_CLEAR_DEPTH, 0x000000ff, 1.0f, 0);
    });
    
    // Single threaded mode - this is crucial!
    render.RenderFrame();
    bgfx::PlatformData pd;
    render.SetPlatformData(window, pd);

    if (!render.Init(window, pd)) {
        return false;
    }

	// Create vertex stream declaration.
	PosColorTexCoord0Vertex::init();

	int64_t m_timeOffset;
	bgfx::UniformHandle u_mtx;
	bgfx::UniformHandle u_lightDirTime;
	bgfx::ProgramHandle m_program;

	u_mtx          = bgfx::createUniform("u_mtx",      bgfx::UniformType::Mat4);
	u_lightDirTime = bgfx::createUniform("u_lightDirTime", bgfx::UniformType::Vec4);
	
	// Create program from embedded shaders.
	bgfx::RendererType::Enum type = bgfx::getRendererType();
	bgfx::ShaderHandle vsh = bgfx::createEmbeddedShader(s_embeddedShaders, type, "vs_raymarching");
	bgfx::ShaderHandle fsh = bgfx::createEmbeddedShader(s_embeddedShaders, type, "fs_raymarching");
	m_program = bgfx::createProgram(vsh, fsh, true);
	
	m_timeOffset = bx::getHPCounter();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(glfwWindow, true);
    ImGui_Implbgfx_Init(1);
	
    // Variables to track window size changes
    int currentWidth = width;
    int currentHeight = height;

    while (!window.WindowShouldClose()) {
        window.HandleWindowEvents();
        
        // Update window size and check for changes
        int newWidth = window.GetFramebufferWidth();
        int newHeight = window.GetFramebufferHeight();
        
        if (newWidth != currentWidth || newHeight != currentHeight) {
            currentWidth = newWidth;
            currentHeight = newHeight;
            width = currentWidth;
            height = currentHeight;
            render.UpdateWindowSize(window);
        }
        
        // Handle window events more frequently
        if (glfwGetKey(glfwWindow, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            window.SetWindowShouldClose(true);
        }
        
        // Check if window needs refresh
        if (window.GetWindowAttrib(GLFW_ICONIFIED)) {
            window.HandleWindowEvents();
            continue; // Skip rendering when minimized
        }

        // ImGui frame setup
        ImGui_ImplGlfw_NewFrame();
        ImGui_Implbgfx_NewFrame();
        ImGui::NewFrame();

        // Settings window
        ImGui::SetNextWindowPos(
            ImVec2(currentWidth - currentWidth / 5.0f - 10.0f, 10.0f),
            ImGuiCond_FirstUseEver
        );
        ImGui::SetNextWindowSize(
            ImVec2(currentWidth / 5.0f, currentHeight / 7.0f),
            ImGuiCond_FirstUseEver
        );
        ImGui::Begin("Settings", NULL, 0);

        // Set view 0 default viewport with current window size
        bgfx::setViewRect(0, 0, 0, uint16_t(currentWidth), uint16_t(currentHeight));

        // Clear the screen with black - do this BEFORE touch(0)
        bgfx::setViewClear(0, BGFX_CLEAR_COLOR|BGFX_CLEAR_DEPTH, 0x000000ff, 1.0f, 0);

        // This dummy draw call is here to make sure that view 0 is cleared
        // if no other draw calls are submitted to view 0.
        bgfx::touch(0);

		const bx::Vec3 at  = { 0.0f, 0.0f,   0.0f };
		const bx::Vec3 eye = { 0.0f, 0.0f, -15.0f };

		float view[16];
		float proj[16];
		bx::mtxLookAt(view, eye, at);

		const bgfx::Caps* caps = bgfx::getCaps();
		bx::mtxProj(proj, 60.0f, float(currentWidth)/float(currentHeight), 0.1f, 100.0f, caps->homogeneousDepth);

		// Set view and projection matrix for view 1.
		bgfx::setViewTransform(0, view, proj);

		float ortho[16];
		bx::mtxOrtho(ortho, 0.0f, 1280.0f, 720.0f, 0.0f, 0.0f, 100.0f, 0.0, caps->homogeneousDepth);

		// Set view and projection matrix for view 0.
		bgfx::setViewTransform(1, NULL, ortho);

		float time = (float)( (bx::getHPCounter()-m_timeOffset)/double(bx::getHPFrequency() ) );

		float vp[16];
		bx::mtxMul(vp, view, proj);

		float mtx[16];
		bx::mtxRotateXY(mtx
			, time
			, time*0.37f
			);

		float mtxInv[16];
		bx::mtxInverse(mtxInv, mtx);
		float lightDirTime[4];
		const bx::Vec3 lightDirModelN = bx::normalize(bx::Vec3{-0.4f, -0.5f, -1.0f});
		bx::store(lightDirTime, bx::mul(lightDirModelN, mtxInv) );
		lightDirTime[3] = time;
		bgfx::setUniform(u_lightDirTime, lightDirTime);

		float mvp[16];
		bx::mtxMul(mvp, mtx, vp);

		float invMvp[16];
		bx::mtxInverse(invMvp, mvp);
		bgfx::setUniform(u_mtx, invMvp);

		renderScreenSpaceQuad(1, m_program, 0.0f, 0.0f, 1280.0f, 720.0f);

        ImGui::End();

        ImGui::Render();
        ImGui_Implbgfx_RenderDrawLists(ImGui::GetDrawData());

        // Advance to next frame. Rendering thread will be kicked to
        // process submitted rendering primitives.
        bgfx::frame();
    }

    ImGui_Implbgfx_Shutdown();
    ImGui_ImplGlfw_Shutdown();

	// Cleanup.
	bgfx::destroy(m_program);
	bgfx::destroy(u_mtx);
	bgfx::destroy(u_lightDirTime);
    ImGui::DestroyContext();
    bgfx::shutdown();
    window.DestroyWindow();
    return true;
}
}
