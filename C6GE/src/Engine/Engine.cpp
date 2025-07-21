#include "Engine.h"
#include "../Window/Window.h"
#include "../Render/Render.h"
#include "../Logging/Log.h"

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
