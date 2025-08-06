#include <glad/glad.h>
#include "Engine.h"
#include <utility>
#include "../Input/Input.h"
#include "../Components/LightComponent.h"
#include "../Components/SpecularTextureComponent.h"
#include <cmath>

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
		CreateObject("cube");
		CreateObject("camera");
		LogObjectInfo(GetObject("square"));
		LogObjectInfo(GetObject("temple"));
		LogObjectInfo(GetObject("cube"));
		LogObjectInfo(GetObject("camera"));


		auto* VertexShader = LoadShader("shader/shader.vert");

		auto* FragmentShader = LoadShader("shader/shader.frag");

		auto* LightVertexShader = LoadShader("shader/light.vert");

		auto* LightFragmentShader = LoadShader("shader/light.frag");


		auto CompiledVertexShader = CompileShader(VertexShader, ShaderType::Vertex);
		auto CompiledFragmentShader = CompileShader(FragmentShader, ShaderType::Fragment);

		auto CompiledLightVertexShader = CompileShader(LightVertexShader, ShaderType::Vertex);
		auto CompiledLightFragmentShader = CompileShader(LightFragmentShader, ShaderType::Fragment);

		auto squareShader = CreateProgram(CompiledVertexShader, CompiledFragmentShader);
		auto lightShader = CreateProgram(CompiledLightVertexShader, CompiledLightFragmentShader);

		AddComponent<ShaderComponent>("square", squareShader);
		AddComponent<ShaderComponent>("temple", squareShader);
		AddComponent<ShaderComponent>("cube", squareShader);

        auto squareMesh = CreateSquare();
        AddComponent<MeshComponent>("square", std::move(squareMesh));

        auto templeMesh = CreateTemple();
        AddComponent<MeshComponent>("temple", std::move(templeMesh));

		auto cubeMesh = CreateCube();
		AddComponent<MeshComponent>("cube", std::move(cubeMesh));

		int width = 0, height = 0, channels = 0;
		auto* diffuseData = LoadTexture("texture/WoodFloor043_2K-JPG/WoodFloor043_2K-JPG_Color.jpg", width, height, channels);
		if (diffuseData) {
			auto diffuseTexture = CreateTexture(diffuseData, width, height, channels);
			AddComponent<TextureComponent>("square", diffuseTexture);
			AddComponent<TextureComponent>("temple", diffuseTexture);
			AddComponent<TextureComponent>("cube", diffuseTexture);
		} else {
			Log(LogLevel::error, "Failed to load diffuse texture");
			// Optionally return false or handle error
		}

		auto* specularData = LoadTexture("texture/WoodFloor043_2K-JPG/WoodFloor043_2K-JPG_Roughness.jpg", width, height, channels);
		if (specularData) {
			auto specularTexture = CreateTexture(specularData, width, height, channels);
			AddComponent<SpecularTextureComponent>("square", specularTexture);
			AddComponent<SpecularTextureComponent>("temple", specularTexture);
			AddComponent<SpecularTextureComponent>("cube", specularTexture);
		} else {
			Log(LogLevel::error, "Failed to load specular texture");
			// Optionally return false or handle error
		}

		AddComponent<TransformComponent>("square", glm::vec3(0.0f, 0.0f, 0.0f));
		AddComponent<TransformComponent>("temple", glm::vec3(2.0f, 0.0f, 0.0f));
		AddComponent<TransformComponent>("cube", glm::vec3(4.0f, 0.0f, 0.0f));

		auto camera = CreateCamera();
		AddComponent<CameraComponent>("camera", *camera);

		CreateObject("light");
		auto lightMesh = CreateCube();
		AddComponent<MeshComponent>("light", std::move(lightMesh));
		AddComponent<TransformComponent>("light", glm::vec3(0.0f, 2.0f, 0.0f)); // Position above
		AddComponent<LightComponent>("light", glm::vec3(1.0f, 1.0f, 1.0f), 1.0f);
		GetComponent<TransformComponent>("light")->Scale = glm::vec3(0.2f);
		AddComponent<ShaderComponent>("light", lightShader);

		// create light 2 with a red color and a different position
		CreateObject("light2");
		auto light2Mesh = CreateCube();
		AddComponent<MeshComponent>("light2", std::move(light2Mesh));
		AddComponent<TransformComponent>("light2", glm::vec3(0.0f, 2.0f, 0.0f)); // Position above
		AddComponent<LightComponent>("light2", glm::vec3(1.0f, 0.0f, 0.0f), 1.0f);
		GetComponent<TransformComponent>("light2")->Scale = glm::vec3(0.2f);
		AddComponent<ShaderComponent>("light2", lightShader);

		return true;
	}

	void Update() {
		float lastTime = glfwGetTime();
		float angle = 0.0f;
		// Main loop: update window and render each frame
		while (IsWindowOpen()) {
			UpdateWindow();
			input::Update();
			Clear(0.2f, 0.3f, 0.3f, 1.0f); // Clear the screen with teal color
        	float currentTime = glfwGetTime();
        	float deltaTime = currentTime - lastTime;
        	lastTime = currentTime;

        	auto* lightTransform = GetComponent<TransformComponent>("light");
        	if (lightTransform) {
            	angle += deltaTime * 1.0f; // Adjust speed as needed
            	lightTransform->Position.x = 3.0f * cos(angle); // Radius 3.0f
            	lightTransform->Position.z = 3.0f * sin(angle);
        	}

			// do the same for light2 but in a diffrent direction
			auto* light2Transform = GetComponent<TransformComponent>("light2");
			if (light2Transform) {
				angle += deltaTime * 1.5f; // Adjust speed as needed
				light2Transform->Position.x = 3.0f * cos(angle + glm::pi<float>()); // Radius 3.0f
				light2Transform->Position.z = 3.0f * sin(angle + glm::pi<float>());
			}

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

			RenderObject("square");
			RenderObject("temple");
			RenderObject("cube");
			RenderObject("light");
			RenderObject("light2");
			Present();
		}
	}

	void Shutdown() {
		// Clean up and close the window
		DestroyWindow();
	}
}