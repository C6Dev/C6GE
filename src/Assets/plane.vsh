#include "structures.fxh"

cbuffer Constants
{
    float4x4 g_CameraViewProj;
    float4x4 g_WorldToShadowMapUVDepth;
    float4   g_LightDirection;
};

void main(in  uint    VertId : SV_VertexID,
          out PlanePSInput PSIn)
{
    float PlaneExtent = 5.0;
    float PlanePos    = -2.0;
    
    float4 Pos[4];
    Pos[0] = float4(-PlaneExtent, PlanePos, -PlaneExtent, 1.0);
    Pos[1] = float4(-PlaneExtent, PlanePos, +PlaneExtent, 1.0);
    Pos[2] = float4(+PlaneExtent, PlanePos, -PlaneExtent, 1.0);
    Pos[3] = float4(+PlaneExtent, PlanePos, +PlaneExtent, 1.0);

    PSIn.Pos          = mul(Pos[VertId], g_CameraViewProj);
    float4 ShadowMapPos = mul(Pos[VertId], g_WorldToShadowMapUVDepth);
    float3 sm = ShadowMapPos.xyz / ShadowMapPos.w;
    // Clamp UVs and depth to [0,1] to avoid sampling outside shadow map and
    // precision issues at edges that may produce stray shadows.
    sm.xy = clamp(sm.xy, 0.0, 1.0);
    sm.z = clamp(sm.z, 0.0, 1.0);
    PSIn.ShadowMapPos = sm;
    PSIn.NdotL        = saturate(dot(float3(0.0, 1.0, 0.0), -g_LightDirection.xyz));
}
