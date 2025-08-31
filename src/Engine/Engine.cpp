#define NOMINMAX

#include "../Window/Window.h"
#include "../Render/Render.h"
#include "../Render/RenderECS.h"
#include "../Render/HDR.h"
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
#include <filesystem>

// Include shader binary data
// #include "glsl/vs_mesh.sc.bin.h"
// #include "essl/vs_mesh.sc.bin.h"
// #include "spirv/vs_mesh.sc.bin.h"
// #include "glsl/fs_mesh.sc.bin.h"
// #include "essl/fs_mesh.sc.bin.h"
// #include "spirv/fs_mesh.sc.bin.h"
#include "glsl/vs_cube.sc.bin.h"
#include "essl/vs_cube.sc.bin.h"
#include "spirv/vs_cube.sc.bin.h"
#include "glsl/fs_cube.sc.bin.h"
#include "essl/fs_cube.sc.bin.h"
#include "spirv/fs_cube.sc.bin.h"

#if defined(_WIN32)
// #include "dx11/vs_mesh.sc.bin.h"
// #include "dx11/fs_mesh.sc.bin.h"
#endif
#if __APPLE__
// #include "metal/vs_mesh.sc.bin.h"
// #include "metal/fs_mesh.sc.bin.h"
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
    // BGFX_EMBEDDED_SHADER(vs_mesh),
    // BGFX_EMBEDDED_SHADER(fs_mesh),
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
    
    // Set up window resize callback (will be updated after HDR system is initialized)
    window.SetFramebufferSizeCallback([](GLFWwindow* window, int width, int height) {
        bgfx::reset(width, height, BGFX_RESET_VSYNC);
        bgfx::setViewRect(0, 0, 0, uint16_t(width), uint16_t(height));
        bgfx::setViewRect(1, 0, 0, uint16_t(width), uint16_t(height));
        bgfx::setViewRect(2, 0, 0, uint16_t(width), uint16_t(height));
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
	
	// Initialize HDR system
	HDR hdrSystem;
	if (!hdrSystem.init(width, height)) {
		std::cout << "Failed to initialize HDR system" << std::endl;
		return false;
	}
	
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
	
	// Load initial texture
	bgfx::TextureHandle orbTexture = textureLoader.loadTexture("assets/textures/normalmap.png");
	if (!textureLoader.isTextureValid(orbTexture)) {
		std::cout << "Failed to load normalmap.png, trying aerial rocks..." << std::endl;
		orbTexture = textureLoader.loadTexture("assets/textures/aerial_rocks_04_diff_2k.jpg");
		if (!textureLoader.isTextureValid(orbTexture)) {
			std::cout << "Failed to load texture" << std::endl;
			return false;
		}
	}
	
	// Create texture uniform
	bgfx::UniformHandle s_texColor = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
	
	// Create HDR uniform
	bgfx::UniformHandle u_hdrParams = bgfx::createUniform("u_hdrParams", bgfx::UniformType::Vec4);
	
	// Create single orb object (no instancing)
	Object orbObject = objectManager.CreateObject("OrbObject");
	
	// Add components one by one to avoid conflicts
	if (!orbObject.HasComponent<Model>()) {
		orbObject.AddComponent<Model>(orbMesh);
	}
	if (!orbObject.HasComponent<Texture>()) {
		orbObject.AddComponent<Texture>(orbTexture);
	}
	if (!orbObject.HasComponent<Material>()) {
		orbObject.AddComponent<Material>(m_program_cube, s_texColor);
	}
	
	// Update transform component (it's already added by CreateObject)
	if (orbObject.HasComponent<Transform>()) {
		auto* transformComp = orbObject.GetComponent<Transform>();
		transformComp->setPosition(0.0f, 0.0f, 0.0f); // Center in front of camera
		transformComp->setRotation(0.0f, 0.0f, 0.0f); // No rotation
		transformComp->setScale(1.0f); // Normal scale
	}
	
	// Get available textures for UI
	std::vector<std::string> availableTextures = textureLoader.getAvailableTextures();
	int currentTextureIndex = 0;
	
	// Find current texture in the list
	for (size_t i = 0; i < availableTextures.size(); ++i) {
		if (availableTextures[i].find("normalmap") != std::string::npos) {
			currentTextureIndex = static_cast<int>(i);
			break;
		}
	}
	
	std::cout << "C6GE Engine started successfully with single static orb" << std::endl;

	// Timing variables
	int64_t m_timeOffset = bx::getHPCounter();
	int64_t m_lastFrameTime = m_timeOffset;
	float m_fps = 0.0f;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(glfwWindow, true);
    ImGui_Implbgfx_Init(2);
	
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
            
            // Resize HDR system
            hdrSystem.resize(currentWidth, currentHeight);
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
            ImVec2(currentWidth / 5.0f, currentHeight / 3.0f),
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
        
        ImGui::Separator();
        
        // Texture swapping section
        ImGui::Text("Texture Swapping");
        ImGui::Text("Current: %s", textureLoader.getTextureName(orbTexture).c_str());
        
        // Create texture selection combo
        if (ImGui::BeginCombo("Select Texture", availableTextures[currentTextureIndex].c_str())) {
            for (int i = 0; i < availableTextures.size(); ++i) {
                const bool isSelected = (currentTextureIndex == i);
                std::string displayName = std::filesystem::path(availableTextures[i]).filename().string();
                
                if (ImGui::Selectable(displayName.c_str(), isSelected)) {
                    if (currentTextureIndex != i) {
                        // Swap texture
                        bgfx::TextureHandle newTexture = textureLoader.reloadTexture(availableTextures[i], orbTexture);
                        if (textureLoader.isTextureValid(newTexture)) {
                            orbTexture = newTexture;
                            currentTextureIndex = i;
                            
                            // Update the object's texture component
                            if (orbObject.HasComponent<Texture>()) {
                                auto* textureComp = orbObject.GetComponent<Texture>();
                                textureComp->handle = orbTexture;
                            }
                        }
                    }
                }
                
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        
        // Quick texture swap buttons
        ImGui::Text("Quick Swap:");
        if (ImGui::Button("Normal Map")) {
            std::string normalPath = "assets/textures/normalmap.png";
            bgfx::TextureHandle newTexture = textureLoader.reloadTexture(normalPath, orbTexture);
            if (textureLoader.isTextureValid(newTexture)) {
                orbTexture = newTexture;
                if (orbObject.HasComponent<Texture>()) {
                    auto* textureComp = orbObject.GetComponent<Texture>();
                    textureComp->handle = orbTexture;
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Aerial Rocks")) {
            std::string aerialPath = "assets/textures/aerial_rocks_04_diff_2k.jpg";
            bgfx::TextureHandle newTexture = textureLoader.reloadTexture(aerialPath, orbTexture);
            if (textureLoader.isTextureValid(newTexture)) {
                orbTexture = newTexture;
                if (orbObject.HasComponent<Texture>()) {
                    auto* textureComp = orbObject.GetComponent<Texture>();
                    textureComp->handle = orbTexture;
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Parallax")) {
            std::string parallaxPath = "assets/textures/parallax-d.png";
            bgfx::TextureHandle newTexture = textureLoader.reloadTexture(parallaxPath, orbTexture);
            if (textureLoader.isTextureValid(newTexture)) {
                orbTexture = newTexture;
                if (orbObject.HasComponent<Texture>()) {
                    auto* textureComp = orbObject.GetComponent<Texture>();
                    textureComp->handle = orbTexture;
                }
            }
        }
        
        ImGui::End();

		ImGui::Begin("Scene Info", NULL, 0);
		ImGui::Text("Single Static Orb");
		ImGui::Text("Position: (0, 0, 0)");
		ImGui::Text("Rotation: (0, 0, 0)");
		ImGui::Text("Scale: 1.0");
		ImGui::Text("Camera: (0, 0, -2.5)");
		ImGui::Text("Rendering: ECS-based");
		ImGui::Text("Model: Orb");
		ImGui::Text("Texture: %s", textureLoader.getTextureName(orbTexture).c_str());
        ImGui::End();
        
        // HDR Settings window
        ImGui::SetNextWindowPos(
            ImVec2(10.0f, 10.0f),
            ImGuiCond_FirstUseEver
        );
        ImGui::SetNextWindowSize(
            ImVec2(300.0f, 200.0f),
            ImGuiCond_FirstUseEver
        );
        ImGui::Begin("HDR Settings", NULL, 0);
        
        static float middleGray = hdrSystem.getMiddleGray();
        static float whitePoint = hdrSystem.getWhitePoint();
        static float threshold = hdrSystem.getThreshold();
        
        if (ImGui::SliderFloat("Middle Gray", &middleGray, 0.1f, 1.0f)) {
            hdrSystem.setMiddleGray(middleGray);
        }
        if (ImGui::SliderFloat("White Point", &whitePoint, 0.1f, 2.0f)) {
            hdrSystem.setWhitePoint(whitePoint);
        }
        if (ImGui::SliderFloat("Threshold", &threshold, 0.1f, 2.0f)) {
            hdrSystem.setThreshold(threshold);
        }
        
        ImGui::Separator();
        ImGui::Text("HDR Parameters:");
        ImGui::Text("Middle Gray: %.3f", hdrSystem.getMiddleGray());
        ImGui::Text("White Point: %.3f", hdrSystem.getWhitePoint());
        ImGui::Text("Threshold: %.3f", hdrSystem.getThreshold());
        
        ImGui::End();

        // Set view 0 to render normally (HDR effects applied via shader)
        bgfx::setViewRect(0, 0, 0, uint16_t(currentWidth), uint16_t(currentHeight));
        bgfx::setViewClear(0, BGFX_CLEAR_COLOR|BGFX_CLEAR_DEPTH, 0x000000ff, 1.0f, 0);

		// Calculate FPS
		int64_t currentTime = bx::getHPCounter();
		double deltaTime = double(currentTime - m_lastFrameTime) / double(bx::getHPFrequency());
		if (deltaTime > 0.0) {
			m_fps = 1.0f / float(deltaTime);
		}
		m_lastFrameTime = currentTime;

		// Set up camera (static position)
		const bx::Vec3 at  = { 0.0f, 0.0f,  0.0f };
		const bx::Vec3 eye = { 0.0f, 0.0f, -2.5f };

		// Set view and projection matrix for view 0.
		float view[16];
		bx::mtxLookAt(view, eye, at);

		float proj[16];
		bx::mtxProj(proj, 60.0f, float(currentWidth)/float(currentHeight), 0.1f, 100.0f, bgfx::getCaps()->homogeneousDepth);
		
		// Set view and projection
		renderECS.SetViewProjection(view, proj);
		bgfx::setViewRect(0, 0, 0, uint16_t(currentWidth), uint16_t(currentHeight) );
		
		// Set HDR parameters based on UI controls
		float exposure = hdrSystem.getMiddleGray();
		float gamma = 2.2f; // Standard gamma
		float whitePointParam = hdrSystem.getWhitePoint();
		float thresholdParam = hdrSystem.getThreshold();
		renderECS.SetHDRParams(exposure, gamma, whitePointParam, thresholdParam);
		
		// Update ECS systems (no animation needed for static orb)
		objectManager.Update();
		
		// Render all objects using the ECS system
		renderECS.RenderAllObjects();

        // Render ImGui on top of the final result
        ImGui::Render();
        ImGui_Implbgfx_RenderDrawLists(ImGui::GetDrawData());

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
    
    // Shutdown HDR system
    hdrSystem.shutdown();

    ImGui_Implbgfx_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    bgfx::shutdown();
    window.DestroyWindow();
    return true;
}
}
