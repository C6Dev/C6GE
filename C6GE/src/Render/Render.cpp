#include "Logging/Log.h"
#define NOMINMAX
#ifdef _WIN32
#include <windows.h>
#endif
#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <glad/glad.h>
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include "Render.h"
#include "../Window/Window.h"
#include "../ECS/Object/Object.h"
#include "../Components/CameraComponent.h"
#include "../Components/LightComponent.h"
#include "../Components/SpecularTextureComponent.h"
#include "../Components/CubemapComponent.h"
#include "../Components/ScaleComponent.h"
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <string>
#include <algorithm>
#include <iostream>
#include "../Components/InstanceComponent.h"
#include "../Logging/Log.h"

GLuint multisampleFBO, resolveFBO, colorTextureMS, colorTextureResolve, depthStencilRBOMS, quadVAO = 0, quadVBO = 0;
extern GLuint postShader;

float quadVertices[] = {
    -1.0f,  1.0f,  0.0f, 1.0f,
    -1.0f, -1.0f,  0.0f, 0.0f,
     1.0f, -1.0f,  1.0f, 0.0f,

    -1.0f,  1.0f,  0.0f, 1.0f,
     1.0f, -1.0f,  1.0f, 0.0f,
     1.0f,  1.0f,  1.0f, 1.0f
};

namespace C6GE {

unsigned int samples = 1; // default (no MSAA)

bool InitRender(unsigned int width, unsigned int height, RendererType render) {
    if (render == RendererType::BGFX) {
		InitBGFX();
        return true;
    } 
    else { // Default: OpenGL
        if (!gladLoadGL()) {
            Log(LogLevel::error, "Failed to initialize GLAD");
            return false;
        }

        GLint maxSamples = 0;
        glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
        samples = static_cast<unsigned int>(maxSamples);
        Log(LogLevel::info, "GPU supports up to " + std::to_string(samples) + "x MSAA");

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_STENCIL_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // Create multisample FBO
        glGenFramebuffers(1, &multisampleFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, multisampleFBO);

        glGenTextures(1, &colorTextureMS);
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, colorTextureMS);
        glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples, GL_RGBA, width, height, GL_TRUE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, colorTextureMS, 0);

        glGenRenderbuffers(1, &depthStencilRBOMS);
        glBindRenderbuffer(GL_RENDERBUFFER, depthStencilRBOMS);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_DEPTH24_STENCIL8, width, height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depthStencilRBOMS);

        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            std::cerr << "[ERROR] Multisample FBO incomplete: " << status << std::endl;
            glDeleteTextures(1, &colorTextureMS);
            glDeleteRenderbuffers(1, &depthStencilRBOMS);
            glDeleteFramebuffers(1, &multisampleFBO);
            return false;
        }

        glGenFramebuffers(1, &resolveFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, resolveFBO);

        glGenTextures(1, &colorTextureResolve);
        glBindTexture(GL_TEXTURE_2D, colorTextureResolve);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTextureResolve, 0);

        status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            std::cerr << "[ERROR] Resolve FBO incomplete: " << status << std::endl;
            glDeleteTextures(1, &colorTextureMS);
            glDeleteTextures(1, &colorTextureResolve);
            glDeleteRenderbuffers(1, &depthStencilRBOMS);
            glDeleteFramebuffers(1, &multisampleFBO);
            glDeleteFramebuffers(1, &resolveFBO);
            return false;
        }

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
}

	// Clear the screen with a specified color
	void Clear(float r, float g, float b, float a) {
		glClearColor(r, g, b, a);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	}

	// Bind the resolve FBO
	void BindNormalFramebuffer() {
		glBindFramebuffer(GL_READ_FRAMEBUFFER, resolveFBO);
	}

	// Unbind the resolve FBO
	void UnbindNormalFramebuffer() {
		glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	}

	// Bind the multisample FBO
	void BindMultisampleFramebuffer() {
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, multisampleFBO);
	}

	// Unbind the multisample FBO
	void UnbindMultisampleFramebuffer() {
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	}

	// Present the rendered frame to the window
	void Present() {
		glBindFramebuffer(GL_READ_FRAMEBUFFER, multisampleFBO);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolveFBO);
		GLint viewport[4];
		glGetIntegerv(GL_VIEWPORT, viewport);
		glBlitFramebuffer(0, 0, viewport[2], viewport[3], 0, 0, viewport[2], viewport[3], GL_COLOR_BUFFER_BIT, GL_LINEAR);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		Clear(0.0f, 0.0f, 0.0f, 1.0f);

		glDisable(GL_DEPTH_TEST);

		glUseProgram(postShader);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, colorTextureResolve);
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
		auto* cubemapComp = GetComponent<CubemapComponent>(name);
		auto* skyboxComp = GetComponent<SkyboxComponent>(name);
    	auto* transform    = GetComponent<TransformComponent>(name); // optional
    	auto* lightComp    = GetComponent<LightComponent>(name);
    	auto* instComp     = GetComponent<InstanceComponent>(name);

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
		if (cubemapComp) {
    		glActiveTexture(GL_TEXTURE2);
    		glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapComp->Cubemap);
    		glUniform1i(glGetUniformLocation(shaderComp->ShaderProgram, "cubemap"), 2);
		}
		if (skyboxComp) {
    		glActiveTexture(GL_TEXTURE3);
    		glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxComp->Cubemap);
    		glUniform1i(glGetUniformLocation(shaderComp->ShaderProgram, "skybox"), 3);
		}
		if (cubemapComp || skyboxComp) {
			glUniform1i(glGetUniformLocation(shaderComp->ShaderProgram, "hasCubemap"), 1);
		} else {
			glUniform1i(glGetUniformLocation(shaderComp->ShaderProgram, "hasCubemap"), 0);
		}

		// Render Skybox
		if (skyboxComp) {
			glDepthFunc(GL_LEQUAL);
            glDisable(GL_CULL_FACE);
            glUseProgram(shaderComp->ShaderProgram);
            auto* cameraComp = GetComponent<CameraComponent>("camera");
            glm::mat4 view = GetViewMatrix(*cameraComp);
            glm::mat4 proj = GetProjectionMatrix();
            glUniformMatrix4fv(glGetUniformLocation(shaderComp->ShaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(glm::mat4(glm::mat3(view))));
            glUniformMatrix4fv(glGetUniformLocation(shaderComp->ShaderProgram, "proj"), 1, GL_FALSE, glm::value_ptr(proj));
            auto* skyboxCubemap = GetComponent<SkyboxComponent>("skybox");
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxCubemap->Cubemap);
            glUniform1i(glGetUniformLocation(shaderComp->ShaderProgram, "skybox"), 0);
            auto* skyboxMeshComp = GetComponent<MeshComponent>("skybox");
            glBindVertexArray(skyboxMeshComp->VAO);
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(skyboxMeshComp->vertexCount), GL_UNSIGNED_INT, 0);
            glBindVertexArray(0);
            glEnable(GL_CULL_FACE);
            glDepthFunc(GL_LESS);
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

    	bool isInstanced = instComp && !instComp->instances.empty();
    	if (!isInstanced) {
    	    GLint modelLoc = glGetUniformLocation(shaderComp->ShaderProgram, "model");
    	    if (modelLoc != -1)
    	        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
    	}

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
    	if (isInstanced) {
    	    if (instComp->instanceVBO == 0) {
    	        glGenBuffers(1, &instComp->instanceVBO);
    	    }
    	    glBindBuffer(GL_ARRAY_BUFFER, instComp->instanceVBO);
    	    glBufferData(GL_ARRAY_BUFFER, instComp->instances.size() * sizeof(glm::mat4), instComp->instances.data(), GL_DYNAMIC_DRAW);

    	    // Assume attributes 0: pos, 1: normal, 2: texcoord, so instance matrix at 3-6
    	    for (unsigned int i = 0; i < 4; ++i) {
    	        unsigned int attrib = 3 + i;
    	        glEnableVertexAttribArray(attrib);
    	        glVertexAttribPointer(attrib, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(i * sizeof(glm::vec4)));
    	        glVertexAttribDivisor(attrib, 1);
    	    }
    	    glBindBuffer(GL_ARRAY_BUFFER, 0);
    	}

		if (modelComp) {
    	    for (auto& mesh : modelComp->meshes) {
    	        glBindVertexArray(mesh.VAO);
    	        if (isInstanced) {
    	            glDrawElementsInstanced(GL_TRIANGLES, static_cast<GLsizei>(mesh.indices.size()), GL_UNSIGNED_INT, 0, static_cast<GLsizei>(instComp->instances.size()));
    	        } else {
    	            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(mesh.indices.size()), GL_UNSIGNED_INT, 0);
    	        }
    	    }
		} else if (meshComp) { // Fallback: single mesh
    	    glBindVertexArray(meshComp->VAO);
    	    if (isInstanced) {
    	        glDrawElementsInstanced(GL_TRIANGLES, static_cast<GLsizei>(meshComp->vertexCount), GL_UNSIGNED_INT, 0, static_cast<GLsizei>(instComp->instances.size()));
    	    } else {
    	        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(meshComp->vertexCount), GL_UNSIGNED_INT, 0);
    	    }
		}
		
    	glBindVertexArray(0);

        // Restore depth test if it was disabled for outline pass
        if (useStencil && isOutlinePass) {
            glEnable(GL_DEPTH_TEST);
        }
    }

    void AddInstance(const std::string& name, const glm::mat4& transform) {
        auto* instComp = GetComponent<InstanceComponent>(name);
        if (!instComp) {
            AddComponent<InstanceComponent>(name);
            instComp = GetComponent<InstanceComponent>(name);
        }
        instComp->instances.push_back(transform);
    }

    void ClearInstances(const std::string& name) {
        auto* instComp = GetComponent<InstanceComponent>(name);
        if (instComp) {
            instComp->instances.clear();
        }
    }

bool InitBGFX() {
    Log(LogLevel::info, "Initializing BGFX...");

    GLFWwindow* window = glfwGetCurrentContext();
    if (!window) {
        Log(LogLevel::error, "No GLFW window available for BGFX.");
        return false;
    }

    bgfx::PlatformData pd{};
#ifdef _WIN32
    pd.nwh = glfwGetWin32Window(window);
#elif __APPLE__
    pd.nwh = glfwGetCocoaWindow(window); // Cocoa NSWindow*
#elif __linux__
    pd.nwh = (void*)(uintptr_t)glfwGetX11Window(window); // X11
#endif
    pd.ndt = nullptr;
    bgfx::setPlatformData(pd);

    bgfx::Init init{};
    init.resolution.width  = 800;  // Temporary test resolution
    init.resolution.height = 600;
    init.resolution.reset  = BGFX_RESET_NONE;

    bool initialized = false;

#ifdef _WIN32
    const bgfx::RendererType::Enum renderers[] = {
        bgfx::RendererType::Direct3D12,
        bgfx::RendererType::Direct3D11,
        bgfx::RendererType::Vulkan,
        bgfx::RendererType::OpenGL
    };
#elif __APPLE__
    const bgfx::RendererType::Enum renderers[] = {
        bgfx::RendererType::Metal,   // Primary on macOS M-series
        bgfx::RendererType::OpenGL   // fallback, may fail on Apple Silicon
    };
#else
    const bgfx::RendererType::Enum renderers[] = {
        bgfx::RendererType::Vulkan,
        bgfx::RendererType::OpenGL
    };
#endif

    for (auto r : renderers) {
        init.type = r;

        if (bgfx::init(init)) {
            Log(LogLevel::info, "BGFX initialized with renderer: " + std::string(bgfx::getRendererName(r)));
            initialized = true;
            break;
        } else {
            Log(LogLevel::warning, "Failed to initialize BGFX with renderer: " + std::string(bgfx::getRendererName(r)));
        }
    }

    if (!initialized) {
        Log(LogLevel::error, "BGFX failed to initialize any renderer.");
        return false;
    }

    // Set debug flags (optional)
    bgfx::setDebug(BGFX_DEBUG_TEXT);
    bgfx::setViewClear(0,
        BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH | BGFX_CLEAR_STENCIL,
        0x303030ff, 1.0f, 0);

    // Simple test frame
    bgfx::touch(0);
    bgfx::frame();

    Log(LogLevel::info, "BGFX test frame rendered successfully. Shutting down...");

    bgfx::shutdown();
    return true;
}
}