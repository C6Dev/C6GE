#pragma once
#include "../Window/Window.h"
#include "../Render/Render.h"
#include "../Logging/Log.h"
#include "../ECS/Object/Object.h"
#include "../Render/Shader/Shader.h"
#include "../Components/ShaderComponent.h"
#include "../Components/MeshComponent.h"
#include "../Render/Texture/Texture.h"
#include "../Components/TextureComponent.h"
#include<glm/glm.hpp>
#include<glm/gtc/matrix_transform.hpp>
#include<glm/gtc/type_ptr.hpp>
#include <GLFW/glfw3.h>

namespace C6GE {
	bool Init();
	void Update();
	void Shutdown();
}