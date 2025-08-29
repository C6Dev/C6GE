#define NOMINMAX

#include "../Window/Window.h"
#include "../Render/Render.h"
#include "../MeshLoader/MeshLoader.h"

#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <bgfx_utils.h>
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
#include <glsl/vs_mesh.sc.bin.h>
#include <essl/vs_mesh.sc.bin.h>
#include <glsl/fs_mesh.sc.bin.h>
#include <essl/fs_mesh.sc.bin.h>
#include <spirv/vs_mesh.sc.bin.h>
#include <spirv/fs_mesh.sc.bin.h>

#if defined(_WIN32)
#include "dx11/vs_mesh.sc.bin.h"
#include "dx11/fs_mesh.sc.bin.h"
#endif
#if __APPLE__
#include "metal/vs_mesh.sc.bin.h"
#include "metal/fs_mesh.sc.bin.h"
#endif

namespace C6GE {

// Embedded shaders - bgfx will automatically select the right format
static const bgfx::EmbeddedShader s_embeddedShaders[] =
{
    BGFX_EMBEDDED_SHADER(vs_mesh),
    BGFX_EMBEDDED_SHADER(fs_mesh),

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

    if (!window.CreateGLFWWindow("C6GE", 1280, 720)) {
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

	int64_t m_timeOffset;
	Mesh* m_mesh;
	bgfx::ProgramHandle m_program;
	bgfx::UniformHandle u_time;
	
	// bunny properties for UI controls
	float m_bunnyScale = 1.0f;
	float m_bunnyRotation = 0.0f;
	bx::Vec3 m_bunnyPosition = {0.0f, 0.0f, 0.0f};

	u_time = bgfx::createUniform("u_time", bgfx::UniformType::Vec4);

	// Create program from shaders.
	bgfx::RendererType::Enum type = bgfx::getRendererType();
	bgfx::ShaderHandle vsh = bgfx::createEmbeddedShader(s_embeddedShaders, type, "vs_mesh");
	bgfx::ShaderHandle fsh = bgfx::createEmbeddedShader(s_embeddedShaders, type, "fs_mesh");
	m_program = bgfx::createProgram(vsh, fsh, true);

	// Pre-convert mesh files to .bin format during initialization
	MeshLoader meshLoader;
	if (meshLoader.preConvertMesh("../src/assets/meshes/bunny.obj")) {
		// Load the converted .bin file
		m_mesh = meshLoader.loadMesh("../src/assets/meshes/bunny.bin");
	} else {
		std::cout << "Failed to pre-convert bunny.obj" << std::endl;
		return false;
	}

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
        
        // Add some content to the settings window
        ImGui::Text("C6GE Engine");
        ImGui::Text("Window Size: %d x %d", currentWidth, currentHeight);
        
        ImGui::End();

		ImGui::Begin("Bunny Settings", NULL, 0);
		ImGui::Text("Bunny Scale: %.2f", m_bunnyScale);
		ImGui::SliderFloat("Bunny Scale", &m_bunnyScale, 0.1f, 10.0f);
		ImGui::SliderFloat("Bunny Rotation", &m_bunnyRotation, 0.0f, 360.0f);
		ImGui::SliderFloat("Bunny Position X", &m_bunnyPosition.x, -10.0f, 10.0f);
		ImGui::SliderFloat("Bunny Position Y", &m_bunnyPosition.y, -10.0f, 10.0f);
		ImGui::SliderFloat("Bunny Position Z", &m_bunnyPosition.z, -10.0f, 10.0f);
		ImGui::End();

        // Set view 0 default viewport with current window size
        bgfx::setViewRect(0, 0, 0, uint16_t(currentWidth), uint16_t(currentHeight));

        // Clear the screen with black - do this BEFORE touch(0)
        bgfx::setViewClear(0, BGFX_CLEAR_COLOR|BGFX_CLEAR_DEPTH, 0x000000ff, 1.0f, 0);

        // Render ImGui
        ImGui::Render();
        ImGui_Implbgfx_RenderDrawLists(ImGui::GetDrawData());

        // This dummy draw call is here to make sure that view 0 is cleared
        // if no other draw calls are submitted to view 0.
        bgfx::touch(0);

		float time = (float)( (bx::getHPCounter()-m_timeOffset)/double(bx::getHPFrequency() ) );
		bgfx::setUniform(u_time, &time);

		const bx::Vec3 at  = { 0.0f, 1.0f,  0.0f };
		const bx::Vec3 eye = { 0.0f, 1.0f, -2.5f };

		// Set view and projection matrix for view 0.
		{
			float view[16];
			bx::mtxLookAt(view, eye, at);

			float proj[16];
			bx::mtxProj(proj, 60.0f, float(currentWidth)/float(currentHeight), 0.1f, 100.0f, bgfx::getCaps()->homogeneousDepth);
			bgfx::setViewTransform(0, view, proj);

			// Set view 0 default viewport.
			bgfx::setViewRect(0, 0, 0, uint16_t(currentWidth), uint16_t(currentHeight) );
		}

		float mtx[16];
		
		// Create transformation matrix using UI-controlled values
		float scaleMtx[16];
		float rotMtx[16];
		float posMtx[16];
		
		// Scale matrix
		bx::mtxScale(scaleMtx, m_bunnyScale, m_bunnyScale, m_bunnyScale);
		
		// Rotation matrix (convert degrees to radians)
		bx::mtxRotateY(rotMtx, bx::toRad(m_bunnyRotation + time*0.37f));
		
		// Position matrix
		bx::mtxTranslate(posMtx, m_bunnyPosition.x, m_bunnyPosition.y, m_bunnyPosition.z);
		
		// Combine transformations: position * rotation * scale
		bx::mtxMul(mtx, posMtx, rotMtx);
		float tempMtx[16];
		bx::mtxMul(tempMtx, mtx, scaleMtx);
		bx::memCopy(mtx, tempMtx, sizeof(mtx));

		// Use custom render state to disable culling and fix "inside out" appearance
		uint64_t renderState = BGFX_STATE_WRITE_RGB
			| BGFX_STATE_WRITE_A
			| BGFX_STATE_WRITE_Z
			| BGFX_STATE_DEPTH_TEST_LESS
			| BGFX_STATE_MSAA;
		// Note: No BGFX_STATE_CULL_* specified = culling disabled
		meshSubmit(m_mesh, 0, m_program, mtx, renderState);

        // Advance to next frame. Rendering thread will be kicked to
        // process submitted rendering primitives.
        bgfx::frame();
    }

    ImGui_Implbgfx_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    bgfx::shutdown();
    window.DestroyWindow();
    return true;
}
}
