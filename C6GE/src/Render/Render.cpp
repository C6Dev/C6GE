#include <glad/glad.h>
#include "Render.h"
#include "../Window/Window.h"
#include "../ECS/Object/Object.h"
#include "../Components/CameraComponent.h"
#include "../Components/LightComponent.h"
#include "../Components/SpecularTextureComponent.h"
#include <glm/gtc/type_ptr.hpp>
#include <vector>

namespace C6GE {
	bool InitRender() {
    	if (!gladLoadGL()) {
        	return false; // OpenGL failed to initialize
    	}

    	glEnable(GL_DEPTH_TEST);
    	glEnable(GL_CULL_FACE);
    	glCullFace(GL_BACK);
    	return true;
	}

	// Clear the screen with a specified color
	void Clear(float r, float g, float b, float a) {
		glClearColor(r, g, b, a);
		glClear(GL_COLOR_BUFFER_BIT	| GL_DEPTH_BUFFER_BIT);
	}

	// Present the rendered frame to the window
	void Present() {
		glfwSwapBuffers(glfwGetCurrentContext());
	}

	void RenderObject(const std::string& name) {
    	auto* shaderComp   = GetComponent<ShaderComponent>(name);
    	auto* meshComp     = GetComponent<MeshComponent>(name);
    	auto* textureComp  = GetComponent<TextureComponent>(name);
    	auto* specularComp = GetComponent<SpecularTextureComponent>(name);
    	auto* transform    = GetComponent<TransformComponent>(name); // optional
    	auto* lightComp    = GetComponent<LightComponent>(name);

    	if (!shaderComp || !meshComp) return;

    	glUseProgram(shaderComp->ShaderProgram);

    	if (lightComp) {
        	glUniform3fv(glGetUniformLocation(shaderComp->ShaderProgram, "lightColor"), 1, glm::value_ptr(lightComp->color));
    	}

    	// Texture binding
    	if (textureComp) {
        	glActiveTexture(GL_TEXTURE0);
        	glBindTexture(GL_TEXTURE_2D, textureComp->Texture);
        	glUniform1i(glGetUniformLocation(shaderComp->ShaderProgram, "tex0"), 0);
    	}

    	if (specularComp) {
        	glActiveTexture(GL_TEXTURE1);
        	glBindTexture(GL_TEXTURE_2D, specularComp->Texture);
        	glUniform1i(glGetUniformLocation(shaderComp->ShaderProgram, "specularMap"), 1);
    	}

    	// Collect light data
    	std::vector<glm::vec3> lightPositions;
    	std::vector<glm::vec3> lightColors;
    	std::vector<float> lightIntensities;
    	auto lightView = registry.view<LightComponent, TransformComponent>();
    	int numLights = 0;
    	for (auto entity : lightView) {
        	auto& light = lightView.get<LightComponent>(entity);
        	auto& trans = lightView.get<TransformComponent>(entity);
        	if (numLights < 4) {
            	lightPositions.push_back(trans.Position);
            	lightColors.push_back(light.color);
            	lightIntensities.push_back(light.intensity);
            	numLights++;
        	}
    	}
    	glUniform1i(glGetUniformLocation(shaderComp->ShaderProgram, "numLights"), numLights);
    	if (numLights > 0) {
        	glUniform3fv(glGetUniformLocation(shaderComp->ShaderProgram, "lightPos"), numLights, glm::value_ptr(lightPositions[0]));
        	glUniform3fv(glGetUniformLocation(shaderComp->ShaderProgram, "lightColor"), numLights, glm::value_ptr(lightColors[0]));
        	glUniform1fv(glGetUniformLocation(shaderComp->ShaderProgram, "lightIntensity"), numLights, lightIntensities.data());
    	}

    	// Construct transform matrix
    	glm::mat4 model = glm::mat4(1.0f);
    	if (transform) {
        	model = glm::translate(model, transform->Position);
        	model = glm::rotate(model, glm::radians(transform->Rotation.x), glm::vec3(1,0,0));
        	model = glm::rotate(model, glm::radians(transform->Rotation.y), glm::vec3(0,1,0));
        	model = glm::rotate(model, glm::radians(transform->Rotation.z), glm::vec3(0,0,1));
        	model = glm::scale(model, transform->Scale);
    	} else {
        	// Default to 2D style (no depth, scale = 1)
        	model = glm::translate(model, glm::vec3(0.0f));
    	}

    	GLint modelLoc = glGetUniformLocation(shaderComp->ShaderProgram, "model");
    	if (modelLoc != -1)
       	glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

		auto* camera = GetComponent<CameraComponent>("camera");
		if (camera) {
			glUniform3fv(glGetUniformLocation(shaderComp->ShaderProgram, "viewPos"), 1, glm::value_ptr(camera->Transform.Position));
		}

		glm::mat4 view = (camera) ? GetViewMatrix(*camera) : glm::mat4(1.0f);
		glm::mat4 proj = glm::perspective(glm::radians(45.0f), 1.0f, 0.1f, 100.0f);

		// Set uniforms
		GLint viewLoc = glGetUniformLocation(shaderComp->ShaderProgram, "view");
		if (viewLoc != -1)
    		glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));

		GLint projLoc = glGetUniformLocation(shaderComp->ShaderProgram, "proj");
		if (projLoc != -1)
    		glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(proj));

    	// Draw
    	glBindVertexArray(meshComp->VAO);
    	glDrawElements(GL_TRIANGLES, meshComp->vertexCount, GL_UNSIGNED_INT, 0);
    	glBindVertexArray(0);
	}



}