#ifdef _WIN32
#include <windows.h>
#endif
// GLAD include removed - using bgfx for OpenGL context management
#include <bgfx/bgfx.h>
#include "Engine.h"
#include <utility>
#include "../Input/Input.h"
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
            
            // Clear the screen with teal color (works for both OpenGL and BGFX)
            Clear(0.2f, 0.3f, 0.3f, 1.0f);

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

            // Render BGFX content (debug text and triangle)
            RenderBGFXCube();

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