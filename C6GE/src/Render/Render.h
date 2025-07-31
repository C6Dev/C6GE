#pragma once
#include <string>
#include "../Components/ShaderComponent.h"
#include "../Components/MeshComponent.h"
#include "../Components/TextureComponent.h"

namespace C6GE {
	bool InitRender();
	void Clear(float r, float g, float b, float a);
	void Present();
	void RenderObject(const std::string& name);
}