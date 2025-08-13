#define NOMINMAX
#ifdef _WIN32
#include <windows.h>
#endif
#include <glad/glad.h>
#include "Render.h"
#include "../Window/Window.h"
#include "../ECS/Object/Object.h"
#include "../Components/CameraComponent.h"
#include "../Components/LightComponent.h"
#include "../Components/SpecularTextureComponent.h"
#include "../Components/ScaleComponent.h"
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <string>
#include <algorithm>

namespace C6GE {
	bool InitRender() {
    	if (!gladLoadGL()) {
        	return false; // OpenGL failed to initialize
    	}

    	glEnable(GL_DEPTH_TEST);
    	// Enable stencil test
    	glEnable(GL_STENCIL_TEST);
    	// Disable face culling to show all faces
    	// glEnable(GL_CULL_FACE);
    	// glCullFace(GL_BACK);
    	return true;
	}

	// Clear the screen with a specified color
	void Clear(float r, float g, float b, float a) {
		glClearColor(r, g, b, a);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	}

	// Present the rendered frame to the window
	void Present() {
		glfwSwapBuffers(glfwGetCurrentContext());
	}

	void RenderObject(const std::string& name, bool useStencil, bool isOutlinePass) {
    	auto* shaderComp   = GetComponent<ShaderComponent>(name);
    	auto* meshComp     = GetComponent<MeshComponent>(name);
		auto* modelComp = GetComponent<ModelComponent>(name);
    	auto* textureComp  = GetComponent<TextureComponent>(name);
    	auto* specularComp = GetComponent<SpecularTextureComponent>(name);
    	auto* transform    = GetComponent<TransformComponent>(name); // optional
    	auto* lightComp    = GetComponent<LightComponent>(name);

    	if (!shaderComp || !meshComp) return;

        // Configure stencil operations based on the current pass
        if (useStencil) {
            if (!isOutlinePass) {
                // First pass: Write to the stencil buffer
                glStencilFunc(GL_ALWAYS, 1, 0xFF); // Always pass stencil test and set stencil value to 1
                glStencilMask(0xFF); // Enable writing to stencil buffer
                glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE); // Replace stencil value on stencil and depth pass
            } else {
                // Second pass: Draw outline using stencil test
                glStencilFunc(GL_NOTEQUAL, 1, 0xFF); // Only pass where stencil is NOT 1 (outside the object)
                glStencilMask(0x00); // Disable writing to stencil buffer
                // Disable depth testing for outline pass to draw over everything
                glDisable(GL_DEPTH_TEST);
            }
        } else {
            // Normal rendering without stencil operations
            glStencilMask(0x00); // Disable writing to stencil buffer
            glStencilFunc(GL_ALWAYS, 1, 0xFF); // Always pass stencil test
        }

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

    	auto* scaleComp = GetComponent<ScaleComponent>(name);
    	if (scaleComp) {
        	model = glm::scale(model, scaleComp->scale);
    	}

    	GLint modelLoc = glGetUniformLocation(shaderComp->ShaderProgram, "model");
    	if (modelLoc != -1)
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

		// Collect all lights
		std::vector<std::string> lightNames = GetAllObjectsWithComponent<LightComponent>();
		int numLights = std::min(static_cast<int>(lightNames.size()), 3); // MAX_LIGHTS = 3
		glUniform1i(glGetUniformLocation(shaderComp->ShaderProgram, "numLights"), numLights);

		for (int i = 0; i < numLights; ++i) {
		    auto* lComp = GetComponent<LightComponent>(lightNames[i]);
		    auto* lTrans = GetComponent<TransformComponent>(lightNames[i]);
		    if (lComp && lTrans) {
		        std::string base = "lights[" + std::to_string(i) + "]";
		        glUniform1i(glGetUniformLocation(shaderComp->ShaderProgram, (base + ".type").c_str()), lComp->type);
		        glUniform3fv(glGetUniformLocation(shaderComp->ShaderProgram, (base + ".position").c_str()), 1, glm::value_ptr(lTrans->Position));
		        glUniform3fv(glGetUniformLocation(shaderComp->ShaderProgram, (base + ".direction").c_str()), 1, glm::value_ptr(lComp->direction));
		        glUniform3fv(glGetUniformLocation(shaderComp->ShaderProgram, (base + ".color").c_str()), 1, glm::value_ptr(lComp->color));
		        glUniform1f(glGetUniformLocation(shaderComp->ShaderProgram, (base + ".intensity").c_str()), lComp->intensity);
		        glUniform1f(glGetUniformLocation(shaderComp->ShaderProgram, (base + ".cutoff").c_str()), lComp->cutoff);
		    }
		}

		auto* camera = GetComponent<CameraComponent>("camera");
		if (camera) {
			glUniform3fv(glGetUniformLocation(shaderComp->ShaderProgram, "viewPos"), 1, glm::value_ptr(camera->Transform.Position));
		}

		glm::mat4 view = (camera) ? GetViewMatrix(*camera) : glm::mat4(1.0f);
		glm::mat4 proj = C6GE::GetProjectionMatrix();

		// Set uniforms
		GLint viewLoc = glGetUniformLocation(shaderComp->ShaderProgram, "view");
		if (viewLoc != -1)
    		glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));

		GLint projLoc = glGetUniformLocation(shaderComp->ShaderProgram, "proj");
		if (projLoc != -1)
    		glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(proj));

    	// Draw

		if (modelComp)
		{
    		for (auto& mesh : modelComp->meshes)
    		{
        		glBindVertexArray(mesh.VAO);
        		glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(mesh.indices.size()), GL_UNSIGNED_INT, 0);
    		}
		}
		else if (meshComp) // Fallback: single mesh
		{
    		glBindVertexArray(meshComp->VAO);
    		glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(meshComp->vertexCount), GL_UNSIGNED_INT, 0);
		}
		
    	glBindVertexArray(0);

        // Restore depth test if it was disabled for outline pass
        if (useStencil && isOutlinePass) {
            glEnable(GL_DEPTH_TEST);
        }
	}



}