#pragma once
#include <string>
#include<glm/glm.hpp>
#include<glm/gtc/matrix_transform.hpp>
#include<glm/gtc/type_ptr.hpp>
#include "../Components/ShaderComponent.h"
#include "../Components/MeshComponent.h"
#include "../Components/ModelComponent.h"
#include "../Components/TextureComponent.h"
#include "../Components/TransformComponent.h"
#include "../Components/InstanceComponent.h"

namespace C6GE {
	
	enum class RendererType {
		OpenGL,
		BGFX
	};

	bool InitRender(unsigned int width, unsigned int height, RendererType renderer);
	RendererType GetCurrentRenderer();
	void Clear(float r, float g, float b, float a);
	void BindNormalFramebuffer();
	void UnbindNormalFramebuffer();
	void BindMultisampleFramebuffer();
	void UnbindMultisampleFramebuffer();
	void Present();
	void RenderObject(const std::string& name, bool useStencil = false, bool isOutlinePass = false);

	void AddInstance(const std::string& name, const glm::mat4& transform);
	void ClearInstances(const std::string& name);

	bool InitBGFX();
	void ClearBGFX(float r, float g, float b, float a);
	void PresentBGFX();
	void UpdateBGFXViewport();
	void WindowResizeCallback(GLFWwindow* window, int width, int height);
	
#ifdef __APPLE__
	bool InitBGFX_macOS(void* windowPtr);
#endif
}
