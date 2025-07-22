#include "Engine.h"
#include "entt/entt.hpp"

namespace C6GE {
	bool Init() {
		if (!CreateWindow(800, 800, "C6GE Window")) {
			Log(LogLevel::critical, "Failed to create window.");
			return false; // Failed to create window
		}

		if (!InitRender()) {
			Log(LogLevel::critical, "Failed to initialize rendering.");
			return false; // Failed to initialize rendering
		}

		CreateObject("object"); // Create an object with the name "object"
		LogObjectInfo(GetObject("object")); // Log the entity information

	struct Transform {
    	float x, y, z;
    	Transform(float x, float y, float z) : x(x), y(y), z(z) {}
	};
        AddComponent<Transform>("object", 0.1f, 0.0f, 0.0f); // Pass as separate arguments

		auto& transform = registry.get<Transform>(GetObject("object"));

		// log transform
		Log(LogLevel::info, "Transform - x: " + std::to_string(transform.x) +
		                     ", y: " + std::to_string(transform.y) +
		                     ", z: " + std::to_string(transform.z));
		return true; // Successfully initialized the engine
	}

	void Update() {
		while (IsWindowOpen()) {
			UpdateWindow();
			Clear(0.2f, 0.3f, 0.3f, 1.0f); // Clear the screen with teal color
			Present();
		}
	}

	void Shutdown() {
		DestroyWindow();
	}
}
