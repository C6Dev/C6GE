#ifdef _WIN32
#include <windows.h>
#endif
#include <glad/glad.h>
#include "Engine.h"
#include <utility>
#include "../Input/Input.h"
#include "../Components/LightComponent.h"
#include "../Components/SpecularTextureComponent.h"
#include "../Components/ScaleComponent.h"
#include <cmath>
#include <vector>

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


		CreateObject("camera");


		auto* VertexShader = LoadShader("Assets/shader.vert");

		auto* FragmentShader = LoadShader("Assets/shader.frag");

		auto* LightVertexShader = LoadShader("Assets/light.vert");

		auto* LightFragmentShader = LoadShader("Assets/light.frag");


		auto CompiledVertexShader = CompileShader(VertexShader, ShaderType::Vertex);
		auto CompiledFragmentShader = CompileShader(FragmentShader, ShaderType::Fragment);

		auto CompiledLightVertexShader = CompileShader(LightVertexShader, ShaderType::Vertex);
		auto CompiledLightFragmentShader = CompileShader(LightFragmentShader, ShaderType::Fragment);

		auto squareShader = CreateProgram(CompiledVertexShader, CompiledFragmentShader);
		auto lightShader = CreateProgram(CompiledLightVertexShader, CompiledLightFragmentShader);

        

		int width = 0, height = 0, channels = 0;
		auto* diffuseData = LoadTexture("Assets/textures/WoodFloor043_2K-JPG_Color.jpg", width, height, channels);
		GLuint diffuseTexture = 0;
		GLuint cubeDiffuseTexture = 0;
		if (diffuseData) {
			diffuseTexture = CreateTexture(diffuseData, width, height, channels);
			
		} else {
			Log(LogLevel::error, "Failed to load diffuse texture");
			// Optionally return false or handle error
		}

		auto* specularData = LoadTexture("Assets/textures/WoodFloor043_2K-JPG_Roughness.jpg", width, height, channels);
		GLuint cubeSpecularTexture = 0;
		GLuint specularTexture = 0;
		if (specularData) {
			specularTexture = CreateTexture(specularData, width, height, channels);
			
		} else {
			Log(LogLevel::error, "Failed to load specular texture");
			// Optionally return false or handle error
		}

		// Create Texture for cube using textures/table/ file name
		auto* cubeDiffuseData = LoadTexture("Assets/textures/round_wooden_table_01_diff_4k.jpg", width, height, channels);
		if (cubeDiffuseData) {
			cubeDiffuseTexture = CreateTexture(cubeDiffuseData, width, height, channels);
		} else {
			Log(LogLevel::error, "Failed to load cube diffuse texture");
			// Optionally return false or handle error
		}
		auto* cubeSpecularData = LoadTexture("Assets/textures/round_wooden_table_01_rough_4k.jpg", width, height, channels);
		if (cubeSpecularData) {
			cubeSpecularTexture = CreateTexture(cubeSpecularData, width, height, channels);
		} else {
			Log(LogLevel::error, "Failed to load cube specular texture");
			// Optionally return false or handle error
		}

		

		auto camera = CreateCamera();
		AddComponent<CameraComponent>("camera", *camera);

		

        // Create 3 floors
        std::vector<std::string> floors = {"floor1", "floor2", "floor3"};
        float floorYs[3] = {0.0f, 5.0f, 10.0f};
        for (int i = 0; i < 3; ++i) {
            CreateObject(floors[i]);
            auto floorMesh = CreateSquare();
            AddComponent<MeshComponent>(floors[i], std::move(floorMesh));
            AddComponent<ShaderComponent>(floors[i], squareShader);
            AddComponent<TextureComponent>(floors[i], diffuseTexture);
            AddComponent<SpecularTextureComponent>(floors[i], specularTexture);
            AddComponent<TransformComponent>(floors[i], glm::vec3(0.0f, floorYs[i], 0.0f));
            GetComponent<TransformComponent>(floors[i])->Rotation = glm::vec3(90.0f, 0.0f, 0.0f); // Flip to horizontal
            AddComponent<ScaleComponent>(floors[i]);
            GetComponent<ScaleComponent>(floors[i])->scale = glm::vec3(10.0f);

            // Add shapes on floor
            std::string sqName = "square" + std::to_string(i+1);
            CreateObject(sqName);
            auto sqMesh = CreateSquare();
            AddComponent<MeshComponent>(sqName, std::move(sqMesh));
            AddComponent<ShaderComponent>(sqName, squareShader);
            AddComponent<TextureComponent>(sqName, diffuseTexture);
            AddComponent<SpecularTextureComponent>(sqName, specularTexture);
            AddComponent<TransformComponent>(sqName, glm::vec3(-4.0f, floorYs[i] + 0.5f, 0.0f));

            std::string cbName = "cube" + std::to_string(i+1);
            CreateObject(cbName);
            auto cbMesh = LoadModel("assets/round_wooden_table_01_4k.fbx");
            AddComponent<MeshComponent>(cbName, std::move(cbMesh));
            AddComponent<ShaderComponent>(cbName, squareShader);
			AddComponent<TextureComponent>(cbName, cubeDiffuseTexture);
			AddComponent<SpecularTextureComponent>(cbName, cubeSpecularTexture);
            AddComponent<TransformComponent>(cbName, glm::vec3(0.0f, floorYs[i] + 0.5f, 0.0f));
			GetComponent<TransformComponent>(cbName)->Rotation = glm::vec3(-90.0f, 0.0f, 0.0f);
			GetComponent<TransformComponent>(cbName)->Position.y -= 0.5f;

            std::string tpName = "temple" + std::to_string(i+1);
            CreateObject(tpName);
            auto tpMesh = CreateTemple();
            AddComponent<MeshComponent>(tpName, std::move(tpMesh));
            AddComponent<ShaderComponent>(tpName, squareShader);
            AddComponent<TextureComponent>(tpName, diffuseTexture);
            AddComponent<SpecularTextureComponent>(tpName, specularTexture);
            AddComponent<TransformComponent>(tpName, glm::vec3(4.0f, floorYs[i] + 0.5f, 0.0f));
        }

        // Add lights for each floor (point for floor1, directional for floor2, spot for floor3)
        // Assuming enum LightType { Point = 0, Directional = 1, Spot = 2 }
        CreateObject("pointLight");
        auto plMesh = CreateCube();
        AddComponent<MeshComponent>("pointLight", std::move(plMesh));
        AddComponent<ShaderComponent>("pointLight", lightShader);
        AddComponent<TransformComponent>("pointLight", glm::vec3(0.0f, 2.0f, 0.0f));
        GetComponent<TransformComponent>("pointLight")->Scale = glm::vec3(0.2f);
        auto& plComp = AddComponent<LightComponent>("pointLight");
		plComp.type = LightType::Point;
		plComp.color = glm::vec3(1.0f);
		plComp.intensity = 1.0f;
		plComp.direction = glm::vec3(0.0f);
		plComp.cutoff = 0.0f;

        CreateObject("dirLight");
        auto dlMesh = CreateCube();
        AddComponent<MeshComponent>("dirLight", std::move(dlMesh));
        AddComponent<ShaderComponent>("dirLight", lightShader);
        AddComponent<TransformComponent>("dirLight", glm::vec3(0.0f, 7.0f, 0.0f));
        GetComponent<TransformComponent>("dirLight")->Scale = glm::vec3(0.2f);
        auto& dlComp = AddComponent<LightComponent>("dirLight");
		dlComp.type = LightType::Directional;
		dlComp.color = glm::vec3(1.0f);
		dlComp.intensity = 1.0f;
		dlComp.direction = glm::vec3(0.0f, -1.0f, 0.0f);
		dlComp.cutoff = 0.0f;

        CreateObject("spotLight");
        auto slMesh = CreateCube();
        AddComponent<MeshComponent>("spotLight", std::move(slMesh));
        AddComponent<ShaderComponent>("spotLight", lightShader);
        AddComponent<TransformComponent>("spotLight", glm::vec3(0.0f, 12.0f, 0.0f));
        GetComponent<TransformComponent>("spotLight")->Scale = glm::vec3(0.2f);
        auto& slComp = AddComponent<LightComponent>("spotLight");
		slComp.type = LightType::Spot;
		slComp.color = glm::vec3(1.0f);
		slComp.intensity = 1.0f;
		slComp.direction = glm::vec3(0.0f, -1.0f, 0.0f);
		slComp.cutoff = static_cast<float>(glm::cos(glm::radians(12.5f)));

		return true;
	}

	void Update() {
		float lastTime = static_cast<float>(glfwGetTime());
		float deltaTime = 0.0f;
		float angle = 0.0f;
		// Main loop: update window and render each frame
		while (IsWindowOpen()) {
			UpdateWindow();
			input::Update();
			Clear(0.2f, 0.3f, 0.3f, 1.0f); // Clear the screen with teal color
        	float currentTime = static_cast<float>(glfwGetTime());
        	deltaTime = currentTime - lastTime;
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

			auto meshObjects = GetAllObjectsWithComponent<MeshComponent>();
            for (const auto& name : meshObjects) {
                RenderObject(name);
            }
			Present();
		}
	}

	void Shutdown() {
		// Clean up and close the window
		DestroyWindow();
	}
}