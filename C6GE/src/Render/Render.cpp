#include <glad/glad.h>
#include "Render.h"
#include "../Window/Window.h"
#include "../ECS/Object/Object.h"

namespace C6GE {
	bool InitRender() {
		return gladLoadGL() != 0; // Returns true if OpenGL is successfully initialized
	}

	// Clear the screen with a specified color
	void Clear(float r, float g, float b, float a) {
		glClearColor(r, g, b, a);
		glClear(GL_COLOR_BUFFER_BIT);
	}

	// Present the rendered frame to the window
	void Present() {
		glfwSwapBuffers(glfwGetCurrentContext());
	}

	void RenderObject(const std::string& name) {
		auto* shaderComp = GetComponent<ShaderComponent>(name);
		auto* meshComp = GetComponent<MeshComponent>(name);

		if (!shaderComp || !meshComp) return;

		glUseProgram(shaderComp->ShaderProgram);
		glBindVertexArray(meshComp->VAO);
		glDrawElements(GL_TRIANGLES, meshComp->vertexCount, GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
	}
}