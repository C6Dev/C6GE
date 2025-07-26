#include <glad/glad.h>
#include "Engine.h"
#include <utility>


using namespace C6GE;

namespace C6GE {

	bool Init() {

	 // Setup triangle data
    float vertices[] = {
        -0.5f, -0.5f, 0.0f,
         0.5f, -0.5f, 0.0f,
         0.0f,  0.5f, 0.0f
    };

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


		CreateObject("triangle");
		LogObjectInfo(GetObject("triangle"));


		auto* VertexShader = LoadShader("shader/shader.vert");

		auto* FragmentShader = LoadShader("shader/shader.frag");


		auto CompiledVertexShader = CompileShader(VertexShader, ShaderType::Vertex);
		auto CompiledFragmentShader = CompileShader(FragmentShader, ShaderType::Fragment);

		auto triangleShader = CreateProgram(CompiledVertexShader, CompiledFragmentShader);

		AddComponent<ShaderComponent>("triangle", triangleShader);

        auto triangleMesh = CreateTriangleMesh();
        AddComponent<MeshComponent>("triangle", std::move(triangleMesh));

		return true;
	}

	void Update() {
		// Main loop: update window and render each frame
		while (IsWindowOpen()) {
			UpdateWindow();
			Clear(0.2f, 0.3f, 0.3f, 1.0f); // Clear the screen with teal color
			RenderObject("triangle");
			Present();
		}
	}

	void Shutdown() {
		// Clean up and close the window
		DestroyWindow();
	}
}