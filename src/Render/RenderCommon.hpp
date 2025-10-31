/*
 *  Copyright 2019-2025 Diligent Graphics LLC
 *  Copyright 2015-2019 Egor Yusov
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#pragma once

#include "Render.h"

#include "MapHelper.hpp"
#include "FileSystem.hpp"
#include "ShaderMacroHelper.hpp"
#include "CommonlyUsedStates.h"
#include "StringTools.hpp"
#include "GraphicsUtilities.h"
#include "GraphicsAccessories.hpp"
#include "AdvancedMath.hpp"
#include "DiligentTools/ThirdParty/imgui/imgui.h"
#include "DiligentTools/ThirdParty/imgui/imgui_internal.h"
#include "DiligentTools/Imgui/interface/ImGuiImplDiligent.hpp"
#include "ImGuizmo.h"

#include "CallbackWrapper.hpp"
#include "Utilities/interface/DiligentFXShaderSourceStreamFactory.hpp"
#include "ShaderSourceFactoryUtils.hpp"
#include "DiligentSamples/Tutorials/Common/src/TexturedCube.hpp"
#include "BasicMath.hpp"
#include "ColorConversion.h"
#include "TextureUtilities.h"

#include "PostFXContext.hpp"
#include "TemporalAntiAliasing.hpp"

#include "Runtime/ECS/World.h"
#include "Runtime/ECS/Components.h"
#include "Render/Systems/RenderSystem.h"
#include "Platform/FileDialog.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace Diligent
{
    namespace HLSL
    {
#include "DiligentFX/Shaders/Common/public/BasicStructures.fxh"
#include "DiligentFX/Shaders/PBR/public/PBR_Structures.fxh"
#include "DiligentFX/Shaders/PBR/private/RenderPBR_Structures.fxh"
    } // namespace HLSL
} // namespace Diligent

using TAASettings = Diligent::HLSL::TemporalAntiAliasingAttribs;

namespace Diligent
{
    extern bool RenderSettingsOpen;
    extern bool RenderShadows;
}

#include "RenderInternals.h"
