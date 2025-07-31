#include <glad/glad.h>
#include "Engine.h"
#include <utility>


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


		CreateObject("square");
		LogObjectInfo(GetObject("square"));


		auto* VertexShader = LoadShader("shader/shader.vert");

		auto* FragmentShader = LoadShader("shader/shader.frag");


		auto CompiledVertexShader = CompileShader(VertexShader, ShaderType::Vertex);
		auto CompiledFragmentShader = CompileShader(FragmentShader, ShaderType::Fragment);

		auto squareShader = CreateProgram(CompiledVertexShader, CompiledFragmentShader);

		AddComponent<ShaderComponent>("square", squareShader);

        auto squareMesh = CreateSquare();
        AddComponent<MeshComponent>("square", std::move(squareMesh));

		int width, height, channels;
		auto* textureData = LoadTexture("texture/texture.png", width = 512, height = 512, channels = 0);

		auto texture = CreateTexture(textureData, width, height, channels);
		AddComponent<TextureComponent>("square", texture);

		return true;
	}

	void Update() {
		// Main loop: update window and render each frame
		while (IsWindowOpen()) {
			UpdateWindow();
			Clear(0.2f, 0.3f, 0.3f, 1.0f); // Clear the screen with teal color
			RenderObject("square");
			Present();
		}
	}

	void Shutdown() {
		// Clean up and close the window
		DestroyWindow();
	}
}