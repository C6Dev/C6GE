// Internal helpers shared across C6GERender translation units.
#pragma once

#include "Render.h"

namespace Diligent::RenderInternals
{
    inline constexpr Uint32 MAX_POINT_LIGHTS = 8;
    inline constexpr Uint32 MAX_SPOT_LIGHTS  = 8;

    struct PointLightCPU
    {
        float3 Position;
        float  Range;
        float3 Color;
        float  Intensity;
    };

    struct SpotLightCPU
    {
        float3 Position;
        float  Range;
        float3 Color;
        float  Intensity;
        float3 Direction;
        float  SpotCos;
    };

    struct Constants
    {
        float4x4 g_World;
        float4x4 g_WorldViewProj;
        float4x4 g_NormalTranform;
        float4   g_DirLight;
        float4   g_Ambient;
        Uint32   g_NumPointLights;
        Uint32   g_NumSpotLights;
        float2   _pad0;
        PointLightCPU g_PointLights[MAX_POINT_LIGHTS];
        SpotLightCPU  g_SpotLights[MAX_SPOT_LIGHTS];
    };

    struct RTConstantsCPU
    {
        float4x4 InvViewProj;
        float4   ViewSize_Plane;
        float4   LightDir_Shadow;
        float4   ShadowSoftParams;
    };
} // namespace Diligent::RenderInternals
