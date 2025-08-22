#ifdef _WIN32
#include <windows.h>
#endif
// GLAD include removed - using bgfx for OpenGL context management
#include <bgfx/bgfx.h>
#include "Engine.h"
#include <utility>
#include "../Input/Input.h"
#include "../Components/ModelComponent.h"
#include "../Components/TransformComponent.h"
#include "../Components/TextureComponent.h"
#include <cmath>
#include <vector>
#include <iomanip>
#include <sstream>
#include <algorithm>

bool MouseCaptured = true;
unsigned int width = 800;
unsigned int height = 800;

namespace C6GE {

    bool Init() {
        // Create the main application window
        if (!CreateWindow(width, height, "C6GE Window")) {
            Log(LogLevel::critical, "Failed to create window.");
            return false;
        }

        // Initialize rendering system
        if (!InitRender(width, height, RendererType::BGFX)) {
            Log(LogLevel::critical, "Failed to initialize rendering.");
            return false;
        }

        input::EnableMouseCapture(true);

        CreateObject("camera");

        auto camera = CreateCamera();
        AddComponent<CameraComponent>("camera", *camera);

        // Create table object with model component and transform
        CreateObject("table");
        auto tableModel = new ModelComponent("Assets/round_wooden_table_01_4k.fbx");
        if (tableModel->LoadModel()) {
            AddComponent<ModelComponent>("table", *tableModel);
            
            // Add transform component and flip the table 90 degrees
            TransformComponent tableTransform;
            tableTransform.Position = glm::vec3(0.0f, 0.0f, 0.0f);
            tableTransform.Rotation = glm::vec3(-90.0f, 0.0f, 0.0f);
            tableTransform.Scale = glm::vec3(1.0f, 1.0f, 1.0f);
            AddComponent<TransformComponent>("table", tableTransform);

            // Add texture component for diffuse + roughness
            TextureComponent tableTex("Assets/textures/round_wooden_table_01_diff_4k.jpg");
            tableTex.roughnessPath = "Assets/textures/round_wooden_table_01_rough_4k.jpg";
            AddComponent<TextureComponent>("table", tableTex);
        } else {
            Log(LogLevel::error, "Engine: Failed to load table model");
            delete tableModel;
        }
            
            // Create a primitive quad object
            CreateObject("quad");
            ModelComponent quadModel;
            quadModel.BuildPrimitive(ModelComponent::PrimitiveType::Quad, 1.0f, 1.0f);
            AddComponent<ModelComponent>("quad", quadModel);
            TransformComponent quadXform;
            quadXform.Position = glm::vec3(-4.0f, 0.0f, -0.5f);
            quadXform.Rotation = glm::vec3(90.0f, 0.0f, 0.0f);
            AddComponent<TransformComponent>("quad", quadXform);
            TextureComponent quadTex("Assets/textures/WoodFloor043_2K-JPG_Color.jpg");
            quadTex.roughnessPath = "Assets/textures/WoodFloor043_2K-JPG_Roughness.jpg";
            AddComponent<TextureComponent>("quad", quadTex);

            // Create a primitive sphere object
            CreateObject("sphere");
            ModelComponent sphereModel;
            sphereModel.BuildPrimitive(ModelComponent::PrimitiveType::Sphere, 1.0f, 24);
            AddComponent<ModelComponent>("sphere", sphereModel);
            TransformComponent sphereXform;
            sphereXform.Position = glm::vec3(-2.0f, 0.5f, 0.0f);
            AddComponent<TransformComponent>("sphere", sphereXform);
            TextureComponent sphereTex("Assets/textures/WoodFloor043_2K-JPG_Color.jpg");
            sphereTex.roughnessPath = "Assets/textures/WoodFloor043_2K-JPG_Roughness.jpg";
            AddComponent<TextureComponent>("sphere", sphereTex);

            // Create a primitive cube object
            CreateObject("cube");
            ModelComponent cubeModel;
            cubeModel.BuildPrimitive(ModelComponent::PrimitiveType::Cube, 1.0f, 1.0f);
            AddComponent<ModelComponent>("cube", cubeModel);
            TransformComponent cubeXform;
            cubeXform.Position = glm::vec3(2.0f, 0.5f, 0.0f);
            AddComponent<TransformComponent>("cube", cubeXform);
            TextureComponent cubeTex("Assets/textures/WoodFloor043_2K-JPG_Color.jpg");
            cubeTex.roughnessPath = "Assets/textures/WoodFloor043_2K-JPG_Roughness.jpg";
            AddComponent<TextureComponent>("cube", cubeTex);

            // Create a floor object
            CreateObject("floor");
            ModelComponent floorModel;
            floorModel.BuildPrimitive(ModelComponent::PrimitiveType::Quad, 1.0f, 1.0f);
            AddComponent<ModelComponent>("floor", floorModel);
            TransformComponent floorXform;
            floorXform.Position = glm::vec3(0.0f, 0.0f, 0.0f);
            floorXform.Scale = glm::vec3(10.0f, 10.0f, 10.0f);
            floorXform.Rotation = glm::vec3(0.0f, 0.0f, 0.0f);
            AddComponent<TransformComponent>("floor", floorXform);
            TextureComponent floorTex("Assets/textures/WoodFloor043_2K-JPG_Color.jpg");
            floorTex.roughnessPath = "Assets/textures/WoodFloor043_2K-JPG_Roughness.jpg";
            AddComponent<TextureComponent>("floor", floorTex);

                // Create table object with model component and transform
                CreateObject("table2");
                auto table2Model = new ModelComponent("Assets/round_wooden_table_01_4k.fbx");
                if (table2Model->LoadModel()) {
                    AddComponent<ModelComponent>("table2", *table2Model);
                    
                    // Add transform component and flip the table 90 degrees
                    TransformComponent table2Transform;
                    table2Transform.Position = glm::vec3(4.0f, 0.0f, 0.0f);
                    table2Transform.Rotation = glm::vec3(-90.0f, 0.0f, 0.0f);
                    table2Transform.Scale = glm::vec3(1.0f, 1.0f, 1.0f);
                    AddComponent<TransformComponent>("table2", table2Transform);
        
                    // Add texture component for diffuse map
                    TextureComponent table2Tex("Assets/textures/round_wooden_table_01_diff_4k.jpg");
                    table2Tex.roughnessPath = "Assets/textures/round_wooden_table_01_rough_4k.jpg";
                    AddComponent<TextureComponent>("table2", table2Tex);
                } else {
                    Log(LogLevel::error, "Engine: Failed to load table model");
                    delete tableModel;
                }

        return true;
    }

    void Update() {
        // FPS smoothing variables
        static const int SAMPLE_COUNT = 10;
        static double deltaTimes[SAMPLE_COUNT] = {0.0};
        static int currentSample = 0;

        // Main loop: update window and render each frame
        while (IsWindowOpen()) {
            UpdateWindow();
            input::Update();
            
            // Update BGFX viewport if needed
            UpdateBGFXViewport();
            
            // Clear the screen with black color (works for both OpenGL and BGFX)
            Clear(0.0f, 0.0f, 0.0f, 1.0f);
            // Begin HDR path for BGFX
            if (GetCurrentRenderer() == RendererType::BGFX) {
                BeginHDR();
            }

            // Calculate delta time once per frame
            double dt = DeltaTime::deltaTime();

            // Store delta time for FPS smoothing
            deltaTimes[currentSample] = dt;
            currentSample = (currentSample + 1) % SAMPLE_COUNT;

            // Calculate smoothed FPS
            double averageDeltaTime = 0.0;
            int validSamples = 0;
            for (int i = 0; i < SAMPLE_COUNT; i++) {
                if (deltaTimes[i] > 0.0) {
                    averageDeltaTime += deltaTimes[i];
                    validSamples++;
                }
            }
            double fps = (validSamples > 0 && averageDeltaTime > 0.0) ? 1.0 / (averageDeltaTime / validSamples) : 0.0;

            // Update window title with smoothed FPS
            std::stringstream ss;
            ss << std::fixed << std::setprecision(1) << "C6GE Window + " << fps << " FPS";
            glfwSetWindowTitle(static_cast<GLFWwindow*>(GetWindow()), ss.str().c_str());

            auto* camera = GetComponent<CameraComponent>("camera");
            // --- Camera Movement ---
            glm::vec3& pos = camera->Transform.Position;
            glm::vec3 front = C6GE::GetCameraFront(camera->Transform);
            glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));
            float speed = camera->MovementSpeed * static_cast<float>(dt);

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

            // Handle transform controls for all objects with TransformComponent
            std::vector<std::string> transformObjects = GetAllObjectsWithComponent<TransformComponent>();
            for (const auto& objName : transformObjects) {
                auto* transform = GetComponent<TransformComponent>(objName);
                if (transform) {
                    float transformSpeed = 2.0f * static_cast<float>(dt);
                    float rotationSpeed = 90.0f * static_cast<float>(dt); // degrees per second
                    
                    // Position controls (using IJKL keys to avoid conflict with camera WASD)
                    if (input::key.j) transform->Translate(glm::vec3(-transformSpeed, 0.0f, 0.0f)); // Move left
                    if (input::key.l) transform->Translate(glm::vec3(transformSpeed, 0.0f, 0.0f));  // Move right
                    if (input::key.i) transform->Translate(glm::vec3(0.0f, transformSpeed, 0.0f));  // Move up
                    if (input::key.k) transform->Translate(glm::vec3(0.0f, -transformSpeed, 0.0f)); // Move down
                    if (input::key.u) transform->Translate(glm::vec3(0.0f, 0.0f, -transformSpeed)); // Move forward
                    if (input::key.o) transform->Translate(glm::vec3(0.0f, 0.0f, transformSpeed));  // Move back
                    
                    // Rotation controls (using arrow keys)
                    if (input::key.up) transform->Rotate(glm::vec3(rotationSpeed, 0.0f, 0.0f)); // Rotate X
                    if (input::key.down) transform->Rotate(glm::vec3(-rotationSpeed, 0.0f, 0.0f)); // Rotate X-
                    if (input::key.left) transform->Rotate(glm::vec3(0.0f, rotationSpeed, 0.0f)); // Rotate Y
                    if (input::key.right) transform->Rotate(glm::vec3(0.0f, -rotationSpeed, 0.0f)); // Rotate Y-
                    
                    // Scale controls
                    if (input::key.minus) transform->ScaleBy(glm::vec3(0.95f)); // Scale down
                    if (input::key.equal) transform->ScaleBy(glm::vec3(1.05f)); // Scale up
                    
                    // Reset position (only for table object)
                    if (input::key.r && objName == "table") {
                        transform->Position = glm::vec3(0.0f, -5.0f, 0.0f);
                        transform->Rotation = glm::vec3(0.0f, 90.0f, 0.0f);
                        transform->Scale = glm::vec3(0.1f, 0.1f, 0.1f);
                    }
                }
            }
            
            // Render BGFX content (debug text and triangle)
            RenderBGFXCube();
            
            // Render only the main table
            RenderBGFXObject("table");
            // Render primitive sphere
            RenderBGFXObject("sphere");
            // Render primitive cube
            RenderBGFXObject("cube");
            // Render primitive quad
            RenderBGFXObject("quad");
            // Render floor
            RenderBGFXObject("floor");
            RenderBGFXObject("table2");
            
            // End HDR and tonemap
            if (GetCurrentRenderer() == RendererType::BGFX) {
                EndHDRAndTonemap();
            }

            // Present the frame (works for both OpenGL and BGFX)
            Present();
        }
    }

    void Shutdown() {
        // Clean up BGFX
        CleanupBGFXResources();
        
        // Clean up and close the window
        DestroyWindow();
    }
}