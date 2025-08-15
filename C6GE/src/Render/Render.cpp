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

GLuint fbo, colorTexture, depthStencilRBO;
GLuint quadVAO = 0, quadVBO = 0;
extern GLuint postShader;

float quadVertices[] = {
    // positions   // texCoords
    -1.0f,  1.0f,  0.0f, 1.0f,
    -1.0f, -1.0f,  0.0f, 0.0f,
     1.0f, -1.0f,  1.0f, 0.0f,

    -1.0f,  1.0f,  0.0f, 1.0f,
     1.0f, -1.0f,  1.0f, 0.0f,
     1.0f,  1.0f,  1.0f, 1.0f
};

namespace C6GE {

	bool InitRender() {
    	if (!gladLoadGL()) {
        	return false;
    	}

    	glEnable(GL_DEPTH_TEST);
    	glEnable(GL_STENCIL_TEST);
    	glEnable(GL_CULL_FACE);
    	glCullFace(GL_BACK);
    	glFrontFace(GL_CCW);
    	glEnable(GL_BLEND);
    	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    	// Create FBO
    	glGenFramebuffers(1, &fbo);
    	glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    	// Create color texture attachment
    	glGenTextures(1, &colorTexture);
    	glBindTexture(GL_TEXTURE_2D, colorTexture);
    	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 800, 800, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr); // Adjust size as needed
    	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0);

    	// Create depth-stencil renderbuffer
    	glGenRenderbuffers(1, &depthStencilRBO);
    	glBindRenderbuffer(GL_RENDERBUFFER, depthStencilRBO);
    	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 800, 800); // Adjust size
    	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depthStencilRBO);

    	// Check FBO status
    	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        // Handle error
        return false;
    }

    	// Unbind FBO
    	glBindFramebuffer(GL_FRAMEBUFFER, 0);

		// Screen quad VAO
		glGenVertexArrays(1, &quadVAO);
		glGenBuffers(1, &quadVBO);
		glBindVertexArray(quadVAO);
		glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
		glBindVertexArray(0);

    	return true;
	}

	// Clear the screen with a specified color
	void Clear(float r, float g, float b, float a) {
		glClearColor(r, g, b, a);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	}

	void BindFramebuffer() {
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	}

	void UnbindFramebuffer() {
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	// Present the rendered frame to the window
	void Present() {
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		Clear(0.0f, 0.0f, 0.0f, 1.0f);

		glDisable(GL_DEPTH_TEST);

		glUseProgram(postShader);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, colorTexture);
		glUniform1i(glGetUniformLocation(postShader, "screenTexture"), 0);

		glBindVertexArray(quadVAO);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		glBindVertexArray(0);

		glEnable(GL_DEPTH_TEST);

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