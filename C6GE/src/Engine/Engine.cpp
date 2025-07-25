#include <glad/glad.h>
#include "Engine.h"
#include "../Components/ShaderComponent.h"


using namespace C6GE;

GLuint VAO, VBO;
GLuint Program;

namespace C6GE {

	bool Init() {

	 // Setup triangle data
    float vertices[] = {
        -0.5f, -0.5f, 0.0f,
         0.5f, -0.5f, 0.0f,
         0.0f,  0.5f, 0.0f
    };

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
		CreateObject("triangle");
		LogObjectInfo(GetObject("triangle"));


		// Add shader components to the "triangle" object
		// Load shaders from files and add them as components
		// Note: This is a temporary test code, will be removed later

		auto* VertexShader = LoadShader("shader/shader.vert");
		AddComponent<VertexShaderComponent>("triangle", VertexShader);

		auto* FragmentShader = LoadShader("shader/shader.frag");
		AddComponent<FragmentShaderComponent>("triangle", FragmentShader);

		// Retrieve and log the VertexShaderComponent
		auto* vertexShaderComp = GetComponent<VertexShaderComponent>("triangle");
		Log(LogLevel::info, "triangle: Vertex Shader: " + std::string(vertexShaderComp->shaderCode));


		// Retrieve and log the FragmentShaderComponent
		auto* fragmentShaderComp = GetComponent<FragmentShaderComponent>("triangle");
		Log(LogLevel::info, "triangle: Fragment Shader: " + std::string(fragmentShaderComp->shaderCode));

		// Compile Shaders
		auto CompiledVertexShader = CompileShader(vertexShaderComp->shaderCode, ShaderType::Vertex);
		auto CompiledFragmentShader = CompileShader(fragmentShaderComp->shaderCode, ShaderType::Fragment);

		// Add Compiled Shaders to Object
		AddComponent<CompiledVertexShaderComponent>("triangle", CompiledVertexShader);
		AddComponent<CompiledFragmentShaderComponent>("triangle", CompiledFragmentShader);

		auto triangleShader = CreateProgram(CompiledVertexShader, CompiledFragmentShader);

		AddComponent<ShaderComponent>("triangle", triangleShader);

		// --- End of Object system test code ---

		// temp to render triangle

		// Generate the VAO and VBO with only 1 object each
		glGenVertexArrays(1, &VAO);
		glGenBuffers(1, &VBO);

		// Make the VAO the current Vertex Array Object by binding it
		glBindVertexArray(VAO);

		// Bind the VBO specifying it's a GL_ARRAY_BUFFER
		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		// Introduce the vertices into the VBO
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

		// Configure the Vertex Attribute so that OpenGL knows how to read the VBO
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
		// Enable the Vertex Attribute so that OpenGL knows to use it
		glEnableVertexAttribArray(0);

		// Bind both the VBO and VAO to 0 so that we don't accidentally modify the VAO and VBO we created
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);

		return true;
	}

	void Update() {
		// Main loop: update window and render each frame
		while (IsWindowOpen()) {
			UpdateWindow();
			Clear(0.2f, 0.3f, 0.3f, 1.0f); // Clear the screen with teal color
			// Tell OpenGL which Shader Program we want to use
			auto triangleShader = GetComponent<ShaderComponent>("triangle")->ShaderProgram;
			glUseProgram(triangleShader);
			// Bind the VAO so OpenGL knows to use it
			glBindVertexArray(VAO);
			// Draw the triangle using the GL_TRIANGLES primitive
			glDrawArrays(GL_TRIANGLES, 0, 3);
			Present();
		}
	}

	void Shutdown() {
		// Clean up and close the window
		DestroyWindow();
	}
}