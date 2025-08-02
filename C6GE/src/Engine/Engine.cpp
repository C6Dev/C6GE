#include <glad/glad.h>
#include "Engine.h"
#include <utility>
#include "../Input/Input.h"

bool MouseCaptured = true;

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

		input::EnableMouseCapture(true);


		CreateObject("square");
		CreateObject("temple");
		CreateObject("camera");
		LogObjectInfo(GetObject("square"));
		LogObjectInfo(GetObject("temple"));
		LogObjectInfo(GetObject("camera"));


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

		AddComponent<TransformComponent>("temple", glm::vec3(2.0f, 0.0f, 0.0f));

		auto camera = CreateCamera();
		AddComponent<CameraComponent>("camera", *camera);

		return true;
	}

	void Update() {
		float lastTime = glfwGetTime();
		// Main loop: update window and render each frame
		while (IsWindowOpen()) {
			UpdateWindow();
			input::Update();
			Clear(0.2f, 0.3f, 0.3f, 1.0f); // Clear the screen with teal color
			RenderObject("square");
        float currentTime = glfwGetTime();
        float deltaTime = currentTime - lastTime;
        lastTime = currentTime;

        auto* camera = GetComponent<CameraComponent>("camera");
        // --- Camera Movement ---
        glm::vec3& pos = camera->Transform.Position;
        glm::vec3 front = C6GE::GetCameraFront(camera->Transform);
        glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));
        float speed = camera->MovementSpeed * deltaTime;

        if (input::key.w)
            pos += speed * front;
        if (input::key.s)
            pos -= speed * front;
        if (input::key.a)
            pos -= speed * right;
        if (input::key.d)
            pos += speed * right;

        // Camera Rotation
        if (camera && MouseCaptured == true) {
            double xoffset = input::mouse.delta_x * camera->MouseSensitivity;
            double yoffset = input::mouse.delta_y * camera->MouseSensitivity;

            camera->Transform.Rotation.y += static_cast<float>(xoffset); // Yaw
            camera->Transform.Rotation.x += static_cast<float>(yoffset); // Pitch

            // Clamp pitch to avoid flipping
            if (camera->Transform.Rotation.x > 89.0f) camera->Transform.Rotation.x = 89.0f;
            if (camera->Transform.Rotation.x < -89.0f) camera->Transform.Rotation.x = -89.0f;
        }

		if (input::key.escape) {
			MouseCaptured = false;
			input::EnableMouseCapture(false);
		}

		if (input::mouse.button1) {
			if (!MouseCaptured) {
				MouseCaptured = true;
				input::EnableMouseCapture(true);
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