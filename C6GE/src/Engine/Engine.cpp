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
#include <iomanip>
#include <sstream>
#include <algorithm>

bool MouseCaptured = true;
GLuint fogShader = 0;
GLuint outlineShader = 0;
GLuint lightShader = 0; // Moved to global scope
GLuint postShader = 0;
bool enableOutline = false; // Toggle for outline effect

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
        lightShader = CreateProgram(CompiledLightVertexShader, CompiledLightFragmentShader); // Assign to global

        // Create Fog Shader using fog.vert and fog.frag
        auto* FogVertexShader = LoadShader("Assets/fog.vert");
        auto* FogFragmentShader = LoadShader("Assets/fog.frag");
        auto CompiledFogVertexShader = CompileShader(FogVertexShader, ShaderType::Vertex);
        auto CompiledFogFragmentShader = CompileShader(FogFragmentShader, ShaderType::Fragment);
        fogShader = CreateProgram(CompiledFogVertexShader, CompiledFogFragmentShader);

        // Create Outline Shader using outline.vert and outline.frag
        auto* OutlineVertexShader = LoadShader("Assets/outline.vert");
        auto* OutlineFragmentShader = LoadShader("Assets/outline.frag");
        auto CompiledOutlineVertexShader = CompileShader(OutlineVertexShader, ShaderType::Vertex);
        auto CompiledOutlineFragmentShader = CompileShader(OutlineFragmentShader, ShaderType::Fragment);
        outlineShader = CreateProgram(CompiledOutlineVertexShader, CompiledOutlineFragmentShader);

        // Create Post-processing Shader using post.vert and post.frag
        auto* PostVertexShader = LoadShader("Assets/post.vert");
        auto* PostFragmentShader = LoadShader("Assets/post.frag");
        auto CompiledPostVertexShader = CompileShader(PostVertexShader, ShaderType::Vertex);
        auto CompiledPostFragmentShader = CompileShader(PostFragmentShader, ShaderType::Fragment);
        postShader = CreateProgram(CompiledPostVertexShader, CompiledPostFragmentShader);

        // set fog uniform values and set frag color to red
        UseProgram(fogShader);
        SetShaderUniformVec3(fogShader, "fogColor", glm::vec3(0.5f, 0.5f, 0.5f));
        SetShaderUniformFloat(fogShader, "fogNear", 1.0f);
        SetShaderUniformFloat(fogShader, "fogFar", 50.0f);
        UseProgram(0); // Unbind the shader program

        int width = 0, height = 0, channels = 0;
        auto* diffuseData = LoadTexture("Assets/textures/WoodFloor043_2K-JPG_Color.jpg", width, height, channels);
        GLuint diffuseTexture = 0;
        GLuint cubeDiffuseTexture = 0;
        if (diffuseData) {
            diffuseTexture = CreateTexture(diffuseData, width, height, channels);
        } else {
            Log(LogLevel::error, "Failed to load diffuse texture");
        }

        auto* specularData = LoadTexture("Assets/textures/WoodFloor043_2K-JPG_Roughness.jpg", width, height, channels);
        GLuint cubeSpecularTexture = 0;
        GLuint specularTexture = 0;
        if (specularData) {
            specularTexture = CreateTexture(specularData, width, height, channels);
        } else {
            Log(LogLevel::error, "Failed to load specular texture");
        }

        // Create Texture for cube using textures/table/ file name
        auto* cubeDiffuseData = LoadTexture("Assets/textures/round_wooden_table_01_diff_4k.jpg", width, height, channels);
        if (cubeDiffuseData) {
            cubeDiffuseTexture = CreateTexture(cubeDiffuseData, width, height, channels);
        } else {
            Log(LogLevel::error, "Failed to load cube diffuse texture");
        }
        auto* cubeSpecularData = LoadTexture("Assets/textures/round_wooden_table_01_rough_4k.jpg", width, height, channels);
        if (cubeSpecularData) {
            cubeSpecularTexture = CreateTexture(cubeSpecularData, width, height, channels);
        } else {
            Log(LogLevel::error, "Failed to load cube specular texture");
        }

        auto* quadData = LoadTexture("Assets/textures/blending_transparent_window.png", width, height, channels);
        GLuint quadTexture = 0;
        if (quadData) {
            quadTexture = CreateTexture(quadData, width, height, channels);
        } else {
            Log(LogLevel::error, "Failed to load quad texture");
        }
        auto* grassData = LoadTexture("Assets/textures/grass.png", width, height, channels);
        GLuint grassTexture = 0;
        if (grassData) {
            grassTexture = CreateTexture(grassData, width, height, channels);
        } else {
            Log(LogLevel::error, "Failed to load grass texture");
        }

        auto camera = CreateCamera();
        AddComponent<CameraComponent>("camera", *camera);

        // Create 3 floors
        std::vector<std::string> floors = {"floor1", "floor2", "floor3"};
        float floorYs[3] = {0.0f, 5.0f, 10.0f};
        for (int i = 0; i < 3; ++i) {
            CreateObject(floors[i]);
            auto floorMesh = CreateQuad();
            AddComponent<MeshComponent>(floors[i], std::move(floorMesh));
            AddComponent<ShaderComponent>(floors[i], fogShader);
            AddComponent<TextureComponent>(floors[i], diffuseTexture);
            AddComponent<SpecularTextureComponent>(floors[i], specularTexture);
            AddComponent<TransformComponent>(floors[i], glm::vec3(0.0f, floorYs[i], 0.0f));
            GetComponent<TransformComponent>(floors[i])->Rotation = glm::vec3(90.0f, 0.0f, 0.0f); // Flip to horizontal
            AddComponent<ScaleComponent>(floors[i]);
            GetComponent<ScaleComponent>(floors[i])->scale = glm::vec3(10.0f);

            // Add shapes on floor
            std::string sqName = "quad" + std::to_string(i+1);
            CreateObject(sqName);
            auto sqMesh = CreateQuad();
            AddComponent<MeshComponent>(sqName, std::move(sqMesh));
            AddComponent<ShaderComponent>(sqName, fogShader);
            AddComponent<TextureComponent>(sqName, quadTexture);
            AddComponent<SpecularTextureComponent>(sqName, specularTexture);
            AddComponent<TransformComponent>(sqName, glm::vec3(-4.0f, floorYs[i] + 0.5f, 0.0f));

            std::string grassName = "quadGrass" + std::to_string(i+1);
            CreateObject(grassName);
            auto grassMesh = CreateQuad();
            AddComponent<MeshComponent>(grassName, std::move(grassMesh));
            AddComponent<ShaderComponent>(grassName, fogShader);
            AddComponent<TextureComponent>(grassName, grassTexture);
            AddComponent<SpecularTextureComponent>(grassName, specularTexture);
            AddComponent<TransformComponent>(grassName, glm::vec3(-4.0f, floorYs[i] + 0.5f, 1.0f));

            std::string tableName = "table" + std::to_string(i+1);
            CreateObject(tableName);
            auto tableMesh = LoadModel("assets/round_wooden_table_01_4k.fbx");
            AddComponent<MeshComponent>(tableName, std::move(tableMesh));
            AddComponent<ShaderComponent>(tableName, fogShader);
            AddComponent<TextureComponent>(tableName, cubeDiffuseTexture);
            AddComponent<SpecularTextureComponent>(tableName, cubeSpecularTexture);
            AddComponent<TransformComponent>(tableName, glm::vec3(0.0f, floorYs[i] + 0.5f, 0.0f));
            GetComponent<TransformComponent>(tableName)->Rotation = glm::vec3(-90.0f, 0.0f, 0.0f);
            GetComponent<TransformComponent>(tableName)->Position.y -= 0.5f;

            std::string tpName = "temple" + std::to_string(i+1);
            CreateObject(tpName);
            auto tpMesh = CreateTemple();
            AddComponent<MeshComponent>(tpName, std::move(tpMesh));
            AddComponent<ShaderComponent>(tpName, fogShader);
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
        // FPS smoothing variables
        static const int SAMPLE_COUNT = 10;
        static double deltaTimes[SAMPLE_COUNT] = {0.0};
        static int currentSample = 0;

        // Main loop: update window and render each frame
        while (IsWindowOpen()) {
            UpdateWindow();
            input::Update();
            BindFramebuffer();
            Clear(0.2f, 0.3f, 0.3f, 1.0f); // Clear the screen with teal color

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
            glfwSetWindowTitle(GetWindow(), ss.str().c_str());

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

            // Always enable outline effect
            enableOutline = true; // Toggle for outline effect

            auto meshObjects = GetAllObjectsWithComponent<MeshComponent>();

            std::vector<std::string> opaque, tables, lights, transparent;
            for(const auto& name : meshObjects){
                if(name.find("quad") != std::string::npos) transparent.push_back(name);
                else if(name.find("table") == 0) tables.push_back(name);
                else if(name.find("Light") != std::string::npos) lights.push_back(name);
                else opaque.push_back(name);
            }

            // Render opaque with fog
            UseProgram(fogShader);
            SetShaderUniformVec3(fogShader, "viewPos", camera->Transform.Position);
            for(const auto& name : opaque) RenderObject(name);

            // Render tables with outline
            for(const auto& name : tables){
                if(enableOutline){
                    // First pass
                    RenderObject(name, true, false);
                    auto* tableShaderComp = GetComponent<ShaderComponent>(name);
                    if(tableShaderComp){
                        GLuint originalShader = tableShaderComp->ShaderProgram;
                        tableShaderComp->ShaderProgram = outlineShader;
                        UseProgram(outlineShader);
                        SetShaderUniformVec3(outlineShader, "outlineColor", glm::vec3(1.0f, 0.5f, 0.0f));
                        SetShaderUniformVec3(outlineShader, "viewPos", camera->Transform.Position);
                        RenderObject(name, true, true);
                        tableShaderComp->ShaderProgram = originalShader;
                    }
                } else {
                    UseProgram(fogShader);
                    RenderObject(name);
                }
            }

            // Render transparent sorted back to front
            std::vector<std::pair<float, std::string>> transItems;
            for(const auto& name : transparent){
                auto* t = GetComponent<TransformComponent>(name);
                if(t){
                    float dist = glm::length(camera->Transform.Position - t->Position);
                    transItems.emplace_back(dist, name);
                }
            }
            std::sort(transItems.rbegin(), transItems.rend());
            UseProgram(fogShader);
            SetShaderUniformVec3(fogShader, "viewPos", camera->Transform.Position);
            glDepthMask(GL_FALSE);
            for(const auto& item : transItems){
                RenderObject(item.second);
            }
            glDepthMask(GL_TRUE);

            // Render lights
            UseProgram(lightShader);
            for(const auto& name : lights){
                RenderObject(name);
            }

            UnbindFramebuffer();
            Present();
        }
    }

    void Shutdown() {
        // Clean up and close the window
        DestroyWindow();
    }
}