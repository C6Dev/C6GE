#pragma once
#include "../Window/Window.h"
#include "../Render/Render.h"
#include "../Logging/Log.h"
#include "../ECS/Object/Object.h"
#include "../Render/Shader/Shader.h"
#include "../Components/ShaderComponent.h"
#include "../Components/MeshComponent.h"

namespace C6GE {
	bool Init();
	void Update();
	void Shutdown();
}