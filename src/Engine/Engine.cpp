#define NOMINMAX

#include "../Window/Window.h"
#include "../Render/Render.h"
#include "../Render/RenderECS.h"
#include "../MeshLoader/MeshLoader.h"
#include "../TextureLoader/TextureLoader.h"
#include "../Object/Object.h"

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
#include <bx/file.h>
#include <bimg/bimg.h>

// Include shader binary data
#include "glsl/vs_mesh.sc.bin.h"
#include "essl/vs_mesh.sc.bin.h"
#include "spirv/vs_mesh.sc.bin.h"
#include "glsl/fs_mesh.sc.bin.h"
#include "essl/fs_mesh.sc.bin.h"
#include "spirv/fs_mesh.sc.bin.h"
#include "glsl/vs_cube.sc.bin.h"
#include "essl/vs_cube.sc.bin.h"
#include "spirv/vs_cube.sc.bin.h"
#include "glsl/fs_cube.sc.bin.h"
#include "essl/fs_cube.sc.bin.h"
#include "spirv/fs_cube.sc.bin.h"

#if defined(_WIN32)
#include "dx11/vs_mesh.sc.bin.h"
#include "dx11/fs_mesh.sc.bin.h"
#endif
#if __APPLE__
#include "metal/vs_mesh.sc.bin.h"
#include "metal/fs_mesh.sc.bin.h"
#include "metal/vs_cube.sc.bin.h"
#include "metal/fs_cube.sc.bin.h"
#endif

namespace C6GE {

// Custom callback class for bgfx
class EngineCallback : public bgfx::CallbackI
{
public:
    virtual ~EngineCallback() {}

    virtual void fatal(const char* _filePath, uint16_t _line, bgfx::Fatal::Enum _code, const char* _str) override
    {
        BX_UNUSED(_filePath, _line);

        // Log fatal error
        std::cout << "Fatal error: 0x" << std::hex << _code << ": " << _str << std::endl;

        // Must terminate, continuing will cause crash anyway.
        abort();
    }

    virtual void traceVargs(const char* _filePath, uint16_t _line, const char* _format, va_list _argList) override
    {
        // Only log if it's an error or warning
        if (strstr(_format, "error") || strstr(_format, "Error") || 
            strstr(_format, "warning") || strstr(_format, "Warning")) {
            std::cout << _filePath << " (" << _line << "): ";
            vprintf(_format, _argList);
            std::cout << std::endl;
        }
    }

    virtual void profilerBegin(const char* /*_name*/, uint32_t /*_abgr*/, const char* /*_filePath*/, uint16_t /*_line*/) override {}
    virtual void profilerBeginLiteral(const char* /*_name*/, uint32_t /*_abgr*/, const char* /*_filePath*/, uint16_t /*_line*/) override {}
    virtual void profilerEnd() override {}

    virtual uint32_t cacheReadSize(uint64_t /*_id*/) override { return 0; }
    virtual bool cacheRead(uint64_t /*_id*/, void* /*_data*/, uint32_t /*_size*/) override { return false; }
    virtual void cacheWrite(uint64_t /*_id*/, const void* /*_data*/, uint32_t /*_size*/) override {}

    virtual void screenShot(const char* _filePath, uint32_t _width, uint32_t _height, uint32_t _pitch, const void* _data, uint32_t /*_size*/, bool _yflip) override
    {
        char temp[1024];
        bx::snprintf(temp, BX_COUNTOF(temp), "%s.png", _filePath);
        
        // Save screenshot as PNG
        bx::FileWriter writer;
        bx::Error err;
        if (bx::open(&writer, temp, false, &err)) {
            bimg::imageWritePng(&writer, _width, _height, _pitch, _data, bimg::TextureFormat::BGRA8, _yflip, &err);
            bx::close(&writer);
            std::cout << "Screenshot saved: " << temp << std::endl;
        }
    }

    virtual void captureBegin(uint32_t /*_width*/, uint32_t /*_height*/, uint32_t /*_pitch*/, bgfx::TextureFormat::Enum /*_format*/, bool /*_yflip*/) override {}
    virtual void captureEnd() override {}
    virtual void captureFrame(const void* /*_data*/, uint32_t /*_size*/) override {}
};

// Embedded shaders - bgfx will automatically select the right format
static const bgfx::EmbeddedShader s_embeddedShaders[] =
{
    BGFX_EMBEDDED_SHADER(vs_mesh),
    BGFX_EMBEDDED_SHADER(fs_mesh),
    BGFX_EMBEDDED_SHADER(vs_cube),
    BGFX_EMBEDDED_SHADER(fs_cube),

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

    // Create custom callback
    EngineCallback callback;
    
    if (!render.Init(window, pd, &callback)) {
        return false;
    }
    
    	// Initialize ECS system
	ObjectManager objectManager;
	RenderECS renderECS(objectManager.GetRegistry());
	
	// Create shaders and materials
	bgfx::RendererType::Enum type = bgfx::getRendererType();
	bgfx::ShaderHandle vsh_cube = bgfx::createEmbeddedShader(s_embeddedShaders, type, "vs_cube");
	bgfx::ShaderHandle fsh_cube = bgfx::createEmbeddedShader(s_embeddedShaders, type, "fs_cube");
	bgfx::ProgramHandle m_program_cube = bgfx::createProgram(vsh_cube, fsh_cube, true);
	
	// Load resources
	MeshLoader meshLoader;
	TextureLoader textureLoader;
	
	// Load orb model
	::Mesh* orbMesh = meshLoader.loadMesh("assets/meshes/orb.bin");
	if (!orbMesh) {
		std::cout << "Failed to load orb.bin mesh, attempting to convert..." << std::endl;
		if (meshLoader.preConvertMesh("assets/meshes/orb.obj")) {
			orbMesh = meshLoader.loadMesh("assets/meshes/orb.bin");
			if (!orbMesh) {
				std::cout << "Failed to load orb.bin mesh after conversion" << std::endl;
				return false;
			}
		}
		else {
			std::cout << "Failed to pre-convert orb.obj" << std::endl;
			return false;
		}
	}
	
	// Load texture
	bgfx::TextureHandle orbTexture = textureLoader.loadTexture("assets/textures/fieldstone-rgba.dds");
	if (!textureLoader.isTextureValid(orbTexture)) {
		std::cout << "Failed to load DDS texture, trying PNG..." << std::endl;
		orbTexture = textureLoader.loadTexture("assets/textures/fieldstone-rgba.png");
		if (!textureLoader.isTextureValid(orbTexture)) {
			std::cout << "Failed to load texture" << std::endl;
			return false;
		}
	}
	
	// Create texture uniform
	bgfx::UniformHandle s_texColor = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
	
	// Create objects using the new ECS system
	Object orbObject = objectManager.CreateObject("OrbObject");
	orbObject.AddComponent<Model>(orbMesh);
	orbObject.AddComponent<Texture>(orbTexture);
	orbObject.AddComponent<Material>(m_program_cube, s_texColor);
	
	// Set up instancing for multiple orbs
	Instanced instanced(100); // 10x10 grid
	const int gridSize = 10;
	for (int i = 0; i < 100; ++i) {
		int row = i / gridSize;
		int col = i % gridSize;
		float x = (float)(col - gridSize/2) * 2.0f; // Closer spacing
		float z = (float)(row - gridSize/2) * 2.0f; // Closer spacing
		float y = 0.0f;
		
		// Create different transformations for each instance
		float scale = 0.5f + (float)(i % 5) * 0.1f; // Larger scale
		float angle = (float)i * 0.1f;
		
		instanced.instances[i].setPosition(x, y, z);
		instanced.instances[i].setRotation(0.0f, angle, 0.0f);
		instanced.instances[i].setScale(scale);
	}
	orbObject.AddComponent<Instanced>(instanced);
	
	std::cout << "C6GE Engine started successfully with ECS system" << std::endl;

	// Timing variables
	int64_t m_timeOffset = bx::getHPCounter();
	int64_t m_lastFrameTime = m_timeOffset;
	float m_fps = 0.0f;



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
        
        // Screenshot with F12 key
        static bool f12Pressed = false;
        if (glfwGetKey(glfwWindow, GLFW_KEY_F12) == GLFW_PRESS) {
            if (!f12Pressed) {
                bgfx::requestScreenShot(BGFX_INVALID_HANDLE, "screenshot");
                f12Pressed = true;
            }
        } else {
            f12Pressed = false;
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
        ImGui::Text("FPS: %.1f", m_fps);
        
        // Screenshot button
        if (ImGui::Button("Take Screenshot")) {
            bgfx::requestScreenShot(BGFX_INVALID_HANDLE, "screenshot");
        }
        ImGui::SameLine();
        ImGui::Text("(or press F12)");
        
        ImGui::End();

			ImGui::Begin("ECS System", NULL, 0);
	ImGui::Text("Entity Component System");
	ImGui::Text("Object: OrbObject");
	ImGui::Text("Components: Transform, Model, Texture, Material, Instanced");
	ImGui::Text("Grid Layout: 10x10");
	ImGui::Text("Total Instances: 100");
	ImGui::Text("Rendering: ECS-based");
	ImGui::Text("Model: Orb");
	ImGui::Text("Texture: Fieldstone");
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

		// Calculate FPS
		int64_t currentTime = bx::getHPCounter();
		double deltaTime = double(currentTime - m_lastFrameTime) / double(bx::getHPFrequency());
		if (deltaTime > 0.0) {
			m_fps = 1.0f / float(deltaTime);
		}
		m_lastFrameTime = currentTime;

		float time = (float)( (currentTime-m_timeOffset)/double(bx::getHPFrequency() ) );

		// Set up camera
		const bx::Vec3 at  = { 0.0f, 0.0f,  0.0f };
		const bx::Vec3 eye = { 3.0f, 4.0f, -3.0f };

		// Set view and projection matrix for view 0.
		float view[16];
		bx::mtxLookAt(view, eye, at);

		float proj[16];
		bx::mtxProj(proj, 60.0f, float(currentWidth)/float(currentHeight), 0.1f, 100.0f, bgfx::getCaps()->homogeneousDepth);
		
		// Set view and projection
		renderECS.SetViewProjection(view, proj);
		bgfx::setViewRect(0, 0, 0, uint16_t(currentWidth), uint16_t(currentHeight) );
		
		// Update object transforms with time-based animation
		Object orbObject = objectManager.GetObject("OrbObject");
		if (orbObject.HasComponent<Instanced>()) {
			auto* instanced = orbObject.GetComponent<Instanced>();
			for (uint32_t i = 0; i < instanced->instanceCount; ++i) {
				if (i < instanced->instances.size()) {
					// Add time-based rotation
					instanced->instances[i].rotation.y = time * 0.5f + (float)i * 0.1f;
				}
			}
		}
		
		// Update ECS systems
		objectManager.Update();
		
		// Render all objects using the ECS system
		renderECS.RenderAllObjects();

        // Advance to next frame. Rendering thread will be kicked to
        // process submitted rendering primitives.
        bgfx::frame();
    }

    // Cleanup bgfx resources
    if (bgfx::isValid(m_program_cube)) {
        bgfx::destroy(m_program_cube);
    }
    if (bgfx::isValid(vsh_cube)) {
        bgfx::destroy(vsh_cube);
    }
    if (bgfx::isValid(fsh_cube)) {
        bgfx::destroy(fsh_cube);
    }
    if (bgfx::isValid(s_texColor)) {
        bgfx::destroy(s_texColor);
    }
    if (bgfx::isValid(orbTexture)) {
        bgfx::destroy(orbTexture);
    }
    
    // Cleanup mesh
    if (orbMesh) {
        meshUnload(orbMesh);
    }
    
    // Clear ECS objects
    objectManager.Clear();

    ImGui_Implbgfx_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    bgfx::shutdown();
    window.DestroyWindow();
    return true;
}
}
