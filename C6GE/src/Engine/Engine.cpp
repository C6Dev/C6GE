#include "Engine.h"
#include "../Components/ShaderComponent.h"


using namespace C6GE;

namespace C6GE {

	bool Init() {
		// Create the main application window
		if (!CreateWindow(800, 800, "C6GE Window")) {
			Log(LogLevel::critical, "Failed to create window.");
			return false;
		}

		// Initialize rendering system
		if (!InitRender()) {
			Log(LogLevel::critical, "Failed to initialize rendering.");
			return false;
		}

		// --- Object system test code (will be removed later) ---
		// Create a test object and log its info
		CreateObject("triangle");
		LogObjectInfo(GetObject("triangle"));


		// Add shader components to the "triangle" object
		// Load shaders from files and add them as components
		// Note: This is a temporary test code, will be removed later

		auto* VertexShader = LoadShader("shader/shader.vert");
		AddComponent<VertexShaderComponent>("triangle", VertexShader);

		auto* FragmentShader = LoadShader("shader/shader.frag");
		AddComponent<FragmentShaderComponent>("triangle", FragmentShader);

		// Retrieve and log the VertexShaderComponent
		auto* vertexShaderComp = GetComponent<VertexShaderComponent>("triangle");
		Log(LogLevel::info, "triangle: Vertex Shader: " + std::string(vertexShaderComp->shaderCode));


		// Retrieve and log the FragmentShaderComponent
		auto* fragmentShaderComp = GetComponent<FragmentShaderComponent>("triangle");
		Log(LogLevel::info, "triangle: Fragment Shader: " + std::string(fragmentShaderComp->shaderCode));

		// --- End of Object system test code ---

		return true;
	}

	void Update() {
		// Main loop: update window and render each frame
		while (IsWindowOpen()) {
			UpdateWindow();
			Clear(0.2f, 0.3f, 0.3f, 1.0f); // Clear the screen with teal color
			Present();
		}
	}

	void Shutdown() {
		// Clean up and close the window
		DestroyWindow();
	}
}