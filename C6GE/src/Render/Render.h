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

namespace C6GE {
	bool InitRender();
	void Clear(float r, float g, float b, float a);
	void BindFramebuffer();
	void UnbindFramebuffer();
	void Present();
	void RenderObject(const std::string& name, bool useStencil = false, bool isOutlinePass = false);
}