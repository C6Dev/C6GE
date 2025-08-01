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
		CreateObject("temple");
		LogObjectInfo(GetObject("square"));
		LogObjectInfo(GetObject("temple"));


		auto* VertexShader = LoadShader("shader/shader.vert");

		auto* FragmentShader = LoadShader("shader/shader.frag");


		auto CompiledVertexShader = CompileShader(VertexShader, ShaderType::Vertex);
		auto CompiledFragmentShader = CompileShader(FragmentShader, ShaderType::Fragment);

		auto squareShader = CreateProgram(CompiledVertexShader, CompiledFragmentShader);

		AddComponent<ShaderComponent>("square", squareShader);
		AddComponent<ShaderComponent>("temple", squareShader);

        auto squareMesh = CreateSquare();
        AddComponent<MeshComponent>("square", std::move(squareMesh));

        auto templeMesh = CreateTemple();
        AddComponent<MeshComponent>("temple", std::move(templeMesh));

		int width, height, channels;
		auto* textureData = LoadTexture("texture/texture.png", width = 512, height = 512, channels = 0);

		auto texture = CreateTexture(textureData, width, height, channels);
		AddComponent<TextureComponent>("square", texture);
		AddComponent<TextureComponent>("temple", texture);

		AddComponent<TransformComponent>("temple", glm::vec3(0.0f, 0.0f, 0.0f));

		return true;
	}

	void Update() {
		// Main loop: update window and render each frame
		while (IsWindowOpen()) {
			UpdateWindow();
			Clear(0.2f, 0.3f, 0.3f, 1.0f); // Clear the screen with teal color
			RenderObject("square");
			// temp to be replace with input system
    		if (glfwGetKey(glfwGetCurrentContext(), GLFW_KEY_W) == GLFW_PRESS) {
				auto* transform = GetComponent<TransformComponent>("temple");
				if (transform) {  // Always check for null!
    				transform->Position.y += 0.01f;
					Log(LogLevel::info, "W is pressed" + std::to_string(transform->Position.y));
				}
    		}
			if (glfwGetKey(glfwGetCurrentContext(), GLFW_KEY_S) == GLFW_PRESS) {
				auto* transform = GetComponent<TransformComponent>("temple");
				if (transform) {  // Always check for null!
    				transform->Position.y -= 0.01f;
					Log(LogLevel::info, "S is pressed" + std::to_string(transform->Position.y));
				}
			}
			if (glfwGetKey(glfwGetCurrentContext(), GLFW_KEY_A) == GLFW_PRESS) {
				auto* transform = GetComponent<TransformComponent>("temple");
				if (transform) {  // Always check for null!
    				transform->Position.x -= 0.01f;
					Log(LogLevel::info, "A is pressed" + std::to_string(transform->Position.x));
				}
			}
			if (glfwGetKey(glfwGetCurrentContext(), GLFW_KEY_D) == GLFW_PRESS) {
				auto* transform = GetComponent<TransformComponent>("temple");
				if (transform) {  // Always check for null!
    				transform->Position.x += 0.01f;
					Log(LogLevel::info, "D is pressed" + std::to_string(transform->Position.x));
				}
			}
			// move position z of object triangle for test
			if (glfwGetKey(glfwGetCurrentContext(), GLFW_KEY_Q) == GLFW_PRESS) {
				auto* transform = GetComponent<TransformComponent>("temple");
				if (transform) {  // Always check for null!
    				transform->Position.z += 0.01f;
					Log(LogLevel::info, "Q is pressed" + std::to_string(transform->Position.z));
				}
			}
			if (glfwGetKey(glfwGetCurrentContext(), GLFW_KEY_E) == GLFW_PRESS) {
				auto* transform = GetComponent<TransformComponent>("temple");
				if (transform) {  // Always check for null!
    				transform->Position.z -= 0.01f;
					Log(LogLevel::info, "E is pressed" + std::to_string(transform->Position.z));
				}
			}
			// move rotation of temple
			if (glfwGetKey(glfwGetCurrentContext(), GLFW_KEY_I) == GLFW_PRESS) {
				auto* transform = GetComponent<TransformComponent>("temple");
				if (transform) {  // Always check for null!
    				transform->Rotation.x += 0.50f;
					Log(LogLevel::info, "A is pressed" + std::to_string(transform->Rotation.z));
				}
			}
			if (glfwGetKey(glfwGetCurrentContext(), GLFW_KEY_K) == GLFW_PRESS) {
				auto* transform = GetComponent<TransformComponent>("temple");
				if (transform) {  // Always check for null!
    				transform->Rotation.x -= 0.50f;
					Log(LogLevel::info, "D is pressed" + std::to_string(transform->Rotation.z));
				}
			}
			if (glfwGetKey(glfwGetCurrentContext(), GLFW_KEY_J) == GLFW_PRESS) {
				auto* transform = GetComponent<TransformComponent>("temple");
				if (transform) {  // Always check for null!
    				transform->Rotation.z += 0.50f;
					Log(LogLevel::info, "A is pressed" + std::to_string(transform->Rotation.z));
				}
			}
			if (glfwGetKey(glfwGetCurrentContext(), GLFW_KEY_L) == GLFW_PRESS) {
				auto* transform = GetComponent<TransformComponent>("temple");
				if (transform) {  // Always check for null!
    				transform->Rotation.z -= 0.50f;
					Log(LogLevel::info, "D is pressed" + std::to_string(transform->Rotation.z));
				}
			}
			RenderObject("temple");
			Present();
		}
	}

	void Shutdown() {
		// Clean up and close the window
		DestroyWindow();
	}
}