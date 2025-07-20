#include "Engine.h"
#include "../Window/Window.h"
#include "../Render/Render.h"

namespace C6GE {
	// Initialize the C6GE engine
	bool Init() {
		// Create a window with specified dimensions and title
		if (!CreateWindow(800, 800, "C6GE Window")) {
			return false; // Failed to create window
		}

		// Initialize rendering
		if (!InitRender()) {
			return false; // Failed to initialize rendering
		}
		return true; // Successfully initialized the engine
	}

	// Update the C6GE engine
	void Update() {
		while (IsWindowOpen()) {
			UpdateWindow(); // Poll events and update the window
			Clear(0.2f, 0.3f, 0.3f, 1.0f); // Clear the screen with teal color
			Present(); // Present the rendered frame
		}
	}

	// Shutdown the C6GE engine
	void Shutdown() {
		DestroyWindow();
	}
}