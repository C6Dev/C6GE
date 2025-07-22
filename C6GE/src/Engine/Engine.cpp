#include "Engine.h"


using namespace C6GE;

// Simple Transform component for testing (Object system)
struct Transform {
	float x, y, z;
	Transform(float x, float y, float z) : x(x), y(y), z(z) {}
};
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
		CreateObject("object");
		LogObjectInfo(GetObject("object"));


		// Add Transform component to the test object
		AddComponent<Transform>("object", 0.1f, 0.0f, 0.0f);

		// Retrieve and log the Transform component
		Transform* transform = GetComponent<Transform>("object");
		if (transform) {
			Log(LogLevel::info, "Transform - x: " + std::to_string(transform->x) +
								", y: " + std::to_string(transform->y) +
								", z: " + std::to_string(transform->z));
		} else {
			Log(LogLevel::warning, "Transform component not found on object.");
		}
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
