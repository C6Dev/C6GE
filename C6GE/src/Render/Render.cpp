#include "Logging/Log.h"
#define NOMINMAX
#ifdef _WIN32
#include <windows.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#endif
#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
// OpenGL headers for OpenGL renderer
#include <GL/gl.h>
#include <GLFW/glfw3.h>
#ifdef __APPLE__
#define GLFW_EXPOSE_NATIVE_COCOA
#endif
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#endif
#include <GLFW/glfw3native.h>

// OpenGL constants that might not be defined in basic headers
#ifndef GL_TEXTURE1
#define GL_TEXTURE1 0x84C1
#endif
#ifndef GL_TEXTURE2
#define GL_TEXTURE2 0x84C2
#endif
#ifndef GL_TEXTURE3
#define GL_TEXTURE3 0x84C3
#endif
#ifndef GL_DYNAMIC_DRAW
#define GL_DYNAMIC_DRAW 0x88E8
#endif

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
#include <cstring>
#include <cstdio>
#include "../Components/InstanceComponent.h"
#include "../Logging/Log.h"
#include "../Window/Window.h"

// BGFX vertex structure
struct BGFXVertex {
    float x, y, z;    // position
    float nx, ny, nz; // normal
    float r, g, b;    // color
    float u, v;       // texture coordinates
};

// BGFX vertex layout
bgfx::VertexLayout bgfxVertexLayout;

// Simple triangle vertices for BGFX (position only) - larger triangle
static float s_triangleVertices[] = {
     0.0f,  0.8f, 0.0f,  // top
    -0.8f, -0.8f, 0.0f,  // bottom left
     0.8f, -0.8f, 0.0f   // bottom right
};

static const uint16_t s_triangleIndices[] = {
    0, 1, 2  // single triangle
};

// BGFX resources
bgfx::VertexBufferHandle bgfxVertexBuffer = BGFX_INVALID_HANDLE;
bgfx::IndexBufferHandle bgfxIndexBuffer = BGFX_INVALID_HANDLE;
bgfx::ProgramHandle bgfxSimpleProgram = BGFX_INVALID_HANDLE;

// Forward declarations
namespace C6GE {
    void InitBGFXResources();
    void RenderBGFXCube();
    bgfx::ShaderHandle CreateShaderFromBinary(const uint8_t* data, uint32_t size, const char* name);
}

// Pre-compiled BGFX shaders for solid color rendering
// These are minimal shaders compiled for DirectX11/OpenGL

// Simple vertex shader (pre-compiled for DX11)
static const uint8_t s_vertexShaderDX11[] = {
    0x56, 0x53, 0x48, 0x0b, 0x00, 0x00, 0x00, 0x00, 0x01, 0x83, 0xf2, 0xe1, 0x01, 0x00, 0x0f, 0x75,
    0x5f, 0x6d, 0x6f, 0x64, 0x65, 0x6c, 0x56, 0x69, 0x65, 0x77, 0x50, 0x72, 0x6f, 0x6a, 0x09, 0x01,
    0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0x02, 0x00, 0x00, 0x44, 0x58, 0x42, 0x43,
    0x26, 0xb0, 0x69, 0x8d, 0x39, 0x3c, 0x9a, 0x36, 0x81, 0x14, 0x89, 0x85, 0x3c, 0x1d, 0x68, 0x54,
    0x01, 0x00, 0x00, 0x00, 0xf8, 0x02, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x2c, 0x00, 0x00, 0x00,
    0x60, 0x00, 0x00, 0x00, 0x94, 0x00, 0x00, 0x00, 0x49, 0x53, 0x47, 0x4e, 0x2c, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x07, 0x00, 0x00,
    0x50, 0x4f, 0x53, 0x49, 0x54, 0x49, 0x4f, 0x4e, 0x00, 0xab, 0xab, 0xab, 0x4f, 0x53, 0x47, 0x4e,
    0x2c, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0f, 0x00, 0x00, 0x00, 0x53, 0x56, 0x5f, 0x50, 0x4f, 0x53, 0x49, 0x54, 0x49, 0x4f, 0x4e, 0x00,
    0x53, 0x48, 0x44, 0x52, 0x5c, 0x02, 0x00, 0x00, 0x40, 0x00, 0x01, 0x00, 0x97, 0x00, 0x00, 0x00,
    0x59, 0x00, 0x00, 0x04, 0x46, 0x8e, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00,
    0x5f, 0x00, 0x00, 0x03, 0x72, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x67, 0x00, 0x00, 0x04,
    0xf2, 0x20, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x68, 0x00, 0x00, 0x02,
    0x01, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00, 0x08, 0x72, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x56, 0x15, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46, 0x82, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x32, 0x00, 0x00, 0x0a, 0x72, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x46, 0x82, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x10, 0x10, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x46, 0x02, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x32, 0x00, 0x00, 0x0a,
    0x72, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46, 0x82, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x02, 0x00, 0x00, 0x00, 0xa6, 0x1a, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46, 0x02, 0x10, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x72, 0x20, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x46, 0x02, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46, 0x82, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x03, 0x00, 0x00, 0x00, 0x36, 0x00, 0x00, 0x08, 0x82, 0x20, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x3a, 0x82, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00
};

// Simple fragment shader (pre-compiled for DX11)
static const uint8_t s_fragmentShaderDX11[] = {
    0x46, 0x53, 0x48, 0x0b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xbc, 0x01, 0x00, 0x00, 0x44, 0x58, 0x42, 0x43, 0x5d, 0x4a, 0x52, 0x44, 0x42, 0x58,
    0x2e, 0x74, 0xaa, 0xb4, 0xd1, 0x67, 0xb8, 0x1e, 0x78, 0xc8, 0x01, 0x00, 0x00, 0x00, 0xbc, 0x01,
    0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x2c, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0xb4, 0x00,
    0x00, 0x00, 0x49, 0x53, 0x47, 0x4e, 0x4c, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x08, 0x00,
    0x00, 0x00, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x03, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x07, 0x07,
    0x00, 0x00, 0x53, 0x56, 0x5f, 0x50, 0x4f, 0x53, 0x49, 0x54, 0x49, 0x4f, 0x4e, 0x00, 0x43, 0x4f,
    0x4c, 0x4f, 0x52, 0x00, 0xab, 0xab, 0x4f, 0x53, 0x47, 0x4e, 0x2c, 0x00, 0x00, 0x00, 0x01, 0x00,
    0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x53, 0x56,
    0x5f, 0x54, 0x41, 0x52, 0x47, 0x45, 0x54, 0x00, 0xab, 0xab, 0x53, 0x48, 0x44, 0x52, 0x00, 0x01,
    0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x62, 0x10, 0x00, 0x03, 0x72, 0x10,
    0x10, 0x00, 0x01, 0x00, 0x00, 0x00, 0x65, 0x00, 0x00, 0x03, 0xf2, 0x20, 0x10, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x36, 0x00, 0x00, 0x05, 0x72, 0x20, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46, 0x12,
    0x10, 0x00, 0x01, 0x00, 0x00, 0x00, 0x36, 0x00, 0x00, 0x05, 0x82, 0x20, 0x10, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x40, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3f, 0x3e, 0x00, 0x00, 0x01, 0x00, 0x00,
    0x00, 0x00
};

// BGFX shader handles
bgfx::ShaderHandle s_vertexShader = BGFX_INVALID_HANDLE;
bgfx::ShaderHandle s_fragmentShader = BGFX_INVALID_HANDLE;

#ifdef __APPLE__
extern "C" void* setupMetalLayer(void* nwh);
#endif

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
RendererType currentRenderer = RendererType::OpenGL; // Track current renderer

bool InitRender(unsigned int width, unsigned int height, RendererType render) {
    currentRenderer = render; // Store the renderer type
    
    if (render == RendererType::BGFX) {
		return InitBGFX();
    } 
    else { // Default: OpenGL
        // OpenGL context should already be available from GLFW
        // No need for GLAD initialization

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
		if (currentRenderer == RendererType::BGFX) {
			ClearBGFX(r, g, b, a);
		} else {
			glClearColor(r, g, b, a);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		}
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
		if (currentRenderer == RendererType::BGFX) {
			PresentBGFX();
		} else {
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

    	// BGFX rendering path
    	if (currentRenderer == RendererType::BGFX) {
    	    // For now, just render a simple cube for any object
    	    RenderBGFXCube();
    	    return;
    	}

    	// OpenGL rendering path continues below...

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

    // Get the window from the Window module instead of current context
    GLFWwindow* window = static_cast<GLFWwindow*>(GetWindow());
    if (!window) {
        Log(LogLevel::error, "No GLFW window available for BGFX.");
        return false;
    }

    // Get window dimensions
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    Log(LogLevel::info, "Window size: " + std::to_string(width) + "x" + std::to_string(height));

    // Make sure window is visible
    glfwShowWindow(window);
    glfwPollEvents(); // Process any pending events

    bgfx::PlatformData pd{};
    pd.ndt = nullptr;
    pd.nwh = nullptr;
    pd.context = nullptr;
    pd.backBuffer = nullptr;
    pd.backBufferDS = nullptr;
    
#ifdef _WIN32
    pd.nwh = glfwGetWin32Window(window);
#elif __APPLE__
    pd.nwh = glfwGetCocoaWindow(window); // Cocoa NSWindow*
    Log(LogLevel::info, "Got Cocoa window handle: " + std::to_string(reinterpret_cast<uintptr_t>(pd.nwh)));
    
    // Set up Metal layer to avoid deadlock issues
    void* metalLayer = setupMetalLayer(pd.nwh);
    Log(LogLevel::info, "Set up Metal layer: " + std::to_string(reinterpret_cast<uintptr_t>(metalLayer)));
#elif __linux__
    pd.nwh = (void*)(uintptr_t)glfwGetX11Window(window); // X11
#endif
    
    if (!pd.nwh) {
        Log(LogLevel::error, "Failed to get native window handle.");
        return false;
    }
    
    Log(LogLevel::info, "Setting BGFX platform data...");
    bgfx::setPlatformData(pd);

    // Try calling renderFrame before init (some platforms require this)
    bgfx::renderFrame();
    
    bgfx::Init init{};
    init.platformData = pd;  // Set platform data directly in init struct
    init.resolution.width  = static_cast<uint32_t>(width);
    init.resolution.height = static_cast<uint32_t>(height);
    init.resolution.reset  = BGFX_RESET_VSYNC;

    bool initialized = false;

#ifdef _WIN32
    // Windows: Prefer DX12 again (as requested), then DX11 -> Vulkan -> OpenGL
    const bgfx::RendererType::Enum renderers[] = {
        bgfx::RendererType::Direct3D12,
        bgfx::RendererType::Direct3D11,
        bgfx::RendererType::Vulkan,
        bgfx::RendererType::OpenGL
    };
#elif __APPLE__
    // macOS: Metal -> OpenGL
    const bgfx::RendererType::Enum renderers[] = {
        bgfx::RendererType::Metal,
        bgfx::RendererType::OpenGL
    };
#else
    // Linux: Vulkan -> OpenGL
    const bgfx::RendererType::Enum renderers[] = {
        bgfx::RendererType::Vulkan,
        bgfx::RendererType::OpenGL
    };
#endif

    for (auto r : renderers) {
        init.type = r;
        Log(LogLevel::info, "Attempting to initialize BGFX with renderer: " + std::string(bgfx::getRendererName(r)));
        
        if (bgfx::init(init)) {
            Log(LogLevel::info, "BGFX initialized successfully with renderer: " + std::string(bgfx::getRendererName(r)));
            initialized = true;
            break;
        } else {
            Log(LogLevel::warning, "Failed to initialize BGFX with renderer: " + std::string(bgfx::getRendererName(r)));
        }
    }

    if (!initialized) {
        Log(LogLevel::error, "BGFX failed to initialize with any renderer.");
        return false;
    }
    
    Log(LogLevel::info, "BGFX initialized with renderer: " + std::string(bgfx::getRendererName(bgfx::getRendererType())));

    // Set debug flags (optional)
    bgfx::setDebug(BGFX_DEBUG_TEXT);
    
    // Set up view 0 with proper clear settings
    bgfx::setViewClear(0,
        BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH | BGFX_CLEAR_STENCIL,
        0x303030ff, 1.0f, 0);
    
    // Set view rect for view 0
    bgfx::setViewRect(0, 0, 0, static_cast<uint16_t>(width), static_cast<uint16_t>(height));

    // Initialize BGFX resources
    InitBGFXResources();

    Log(LogLevel::info, "BGFX initialization complete.");
    return true;
}

void InitBGFXResources() {
    // Set up simple vertex layout (position only)
    bgfxVertexLayout
        .begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .end();

    // Create vertex buffer
    bgfxVertexBuffer = bgfx::createVertexBuffer(
        bgfx::makeRef(s_triangleVertices, sizeof(s_triangleVertices)),
        bgfxVertexLayout
    );

    // Create index buffer
    bgfxIndexBuffer = bgfx::createIndexBuffer(
        bgfx::makeRef(s_triangleIndices, sizeof(s_triangleIndices))
    );

    // Try to load compiled shaders first
    Log(LogLevel::info, "Attempting to load compiled shaders (.bin)...");
    
    // Try to load vertex shader from compiled file
    FILE* vsFile = fopen("Assets/shaders/test.vs.bin", "rb");
    if (vsFile) {
        fseek(vsFile, 0, SEEK_END);
        long vsSize = ftell(vsFile);
        fseek(vsFile, 0, SEEK_SET);
        
        char* vsSource = new char[vsSize];
        fread(vsSource, 1, vsSize, vsFile);
        fclose(vsFile);
        Log(LogLevel::info, "Loaded vertex shader from file, size: " + std::to_string(vsSize));
        const bgfx::Memory* vsMem = bgfx::alloc((uint32_t)vsSize);
        memcpy(vsMem->data, vsSource, vsSize);
        s_vertexShader = bgfx::createShader(vsMem);
        
        delete[] vsSource;
    } else {
        Log(LogLevel::warning, "Could not open compiled vertex shader file, will fallback.");
        
        // Fallback to embedded shader
        static const char* vertexShaderSource = R"(
            attribute vec3 a_position;
            void main() {
                gl_Position = vec4(a_position, 1.0);
            }
        )";
        
        const bgfx::Memory* vsMem = bgfx::alloc(strlen(vertexShaderSource) + 1);
        strcpy((char*)vsMem->data, vertexShaderSource);
        s_vertexShader = bgfx::createShader(vsMem);
    }
    
    // Try to load fragment shader from compiled file
    FILE* fsFile = fopen("Assets/shaders/test.fs.bin", "rb");
    if (fsFile) {
        fseek(fsFile, 0, SEEK_END);
        long fsSize = ftell(fsFile);
        fseek(fsFile, 0, SEEK_SET);
        
        char* fsSource = new char[fsSize];
        fread(fsSource, 1, fsSize, fsFile);
        fclose(fsFile);
        Log(LogLevel::info, "Loaded fragment shader from file, size: " + std::to_string(fsSize));
        const bgfx::Memory* fsMem = bgfx::alloc((uint32_t)fsSize);
        memcpy(fsMem->data, fsSource, fsSize);
        s_fragmentShader = bgfx::createShader(fsMem);
        
        delete[] fsSource;
    } else {
        Log(LogLevel::warning, "Could not open compiled fragment shader file, will fallback.");
        
        // Fallback to embedded shader
        static const char* fragmentShaderSource = R"(
            void main() {
                gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);
            }
        )";
        
        const bgfx::Memory* fsMem = bgfx::alloc(strlen(fragmentShaderSource) + 1);
        strcpy((char*)fsMem->data, fragmentShaderSource);
        s_fragmentShader = bgfx::createShader(fsMem);
    }
    
    // Try to create shader program
    Log(LogLevel::info, "Creating shader program...");
    Log(LogLevel::info, "Vertex shader handle valid: " + std::string(bgfx::isValid(s_vertexShader) ? "Yes" : "No"));
    Log(LogLevel::info, "Fragment shader handle valid: " + std::string(bgfx::isValid(s_fragmentShader) ? "Yes" : "No"));
    
    if (bgfx::isValid(s_vertexShader) && bgfx::isValid(s_fragmentShader)) {
        bgfxSimpleProgram = bgfx::createProgram(s_vertexShader, s_fragmentShader, true);
        Log(LogLevel::info, "BGFX shader program created successfully.");
        Log(LogLevel::info, "Program valid: " + std::string(bgfx::isValid(bgfxSimpleProgram) ? "Yes" : "No"));
    } else {
        Log(LogLevel::error, "Failed to create BGFX shader program.");
    }

    Log(LogLevel::info, "BGFX resources initialized.");
}

bgfx::ShaderHandle CreateShaderFromBinary(const uint8_t* data, uint32_t size, const char* name) {
    // Create a memory reference for the pre-compiled shader binary
    const bgfx::Memory* mem = bgfx::makeRef(data, size);
    
    // Create the shader handle from binary data
    bgfx::ShaderHandle handle = bgfx::createShader(mem);
    
    if (bgfx::isValid(handle)) {
        Log(LogLevel::info, "Created BGFX shader from binary: " + std::string(name));
    } else {
        Log(LogLevel::error, "Failed to create BGFX shader from binary: " + std::string(name));
    }
    
    return handle;
}

void RenderBGFXCube() {
    // Clear debug text area first to prevent duplicates on resize
    bgfx::dbgTextClear();
    
    // Basic status info
    bgfx::dbgTextPrintf(0, 1, 0x0f, "BGFX Status:");
    bgfx::dbgTextPrintf(0, 2, 0x0f, "VB: %s, IB: %s, Shader: %s", 
        bgfx::isValid(bgfxVertexBuffer) ? "OK" : "NO",
        bgfx::isValid(bgfxIndexBuffer) ? "OK" : "NO",
        bgfx::isValid(bgfxSimpleProgram) ? "OK" : "NO");
    bgfx::dbgTextPrintf(0, 3, 0x0f, "Renderer: %s", bgfx::getRendererName(bgfx::getRendererType()));
    
    // Calculate and display FPS
    static int frameCount = 0;
    static float lastTime = 0.0f;
    frameCount++;
    
    float currentTime = static_cast<float>(glfwGetTime());
    if (currentTime - lastTime >= 1.0f) {
        float fps = static_cast<float>(frameCount) / (currentTime - lastTime);
        bgfx::dbgTextPrintf(0, 4, 0x0f, "FPS: %.1f", fps);
        frameCount = 0;
        lastTime = currentTime;
    }
    
    // Render a simple triangle if we have valid shaders
    if (bgfx::isValid(bgfxSimpleProgram)) {
        bgfx::dbgTextPrintf(0, 6, 0x0a, "RENDERING TRIANGLE:");
        bgfx::dbgTextPrintf(0, 7, 0x0a, "Shader program is valid!");
        bgfx::dbgTextPrintf(0, 8, 0x0a, "Drawing red triangle at origin");
        
        // Create a dummy vertex buffer with 3 vertices for the draw call
        static bool bufferCreated = false;
        static bgfx::VertexBufferHandle dummyVB = BGFX_INVALID_HANDLE;
        
        if (!bufferCreated) {
            // Create a simple vertex buffer with 3 dummy vertices
            float dummyVertices[] = { 0.0f, 0.0f, 0.0f,  1.0f, 0.0f, 0.0f,  2.0f, 0.0f, 0.0f };
            bgfx::VertexLayout dummyLayout;
            dummyLayout.begin()
                .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
                .end();
            dummyVB = bgfx::createVertexBuffer(bgfx::makeRef(dummyVertices, sizeof(dummyVertices)), dummyLayout);
            bufferCreated = true;
        }
        
        // Set the dummy vertex buffer (shader will ignore it and use gl_VertexID)
        bgfx::setVertexBuffer(0, dummyVB);
        
        // Set render state (no culling to ensure triangle is visible)
        bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z);
        
        // Submit the draw call
        bgfx::submit(0, bgfxSimpleProgram);
    } else {
        bgfx::dbgTextPrintf(0, 6, 0x0c, "SHADER ISSUE:");
        bgfx::dbgTextPrintf(0, 7, 0x0c, "Shader program is not valid");
        bgfx::dbgTextPrintf(0, 8, 0x0c, "Cannot render 3D geometry");
    }
}

// BGFX-specific rendering functions
void ClearBGFX(float r, float g, float b, float a) {
    // Convert float values (0.0-1.0) to RGBA8 format
    uint32_t rgba = 0;
    rgba |= (uint32_t)(r * 255.0f) << 24;  // R
    rgba |= (uint32_t)(g * 255.0f) << 16;  // G
    rgba |= (uint32_t)(b * 255.0f) << 8;   // B
    rgba |= (uint32_t)(a * 255.0f);        // A
    
    // Set the clear color for view 0
    bgfx::setViewClear(0, 
        BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH | BGFX_CLEAR_STENCIL,
        rgba, 1.0f, 0);
    
    // Touch the view to ensure it gets cleared
    bgfx::touch(0);
}

void PresentBGFX() {
    // Submit the frame - this is crucial for proper frame timing
    bgfx::frame();
    
    // Also ensure we're processing events
    GLFWwindow* window = static_cast<GLFWwindow*>(GetWindow());
    if (window) {
        glfwPollEvents();
    }
}

void UpdateBGFXViewport() {
    GLFWwindow* window = glfwGetCurrentContext();
    if (window) {
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        bgfx::reset(static_cast<uint32_t>(width), static_cast<uint32_t>(height), BGFX_RESET_VSYNC);
        bgfx::setViewRect(0, 0, 0, static_cast<uint16_t>(width), static_cast<uint16_t>(height));
    }
}

void WindowResizeCallback(GLFWwindow* window, int width, int height) {
    if (currentRenderer == RendererType::BGFX) {
        bgfx::reset(static_cast<uint32_t>(width), static_cast<uint32_t>(height), BGFX_RESET_VSYNC);
        bgfx::setViewRect(0, 0, 0, static_cast<uint16_t>(width), static_cast<uint16_t>(height));
    }
}

RendererType GetCurrentRenderer() {
    return currentRenderer;
}
}