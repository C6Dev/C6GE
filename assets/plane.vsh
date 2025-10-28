cbuffer Constants
{
    float4x4 g_CameraViewProj;
    float4x4 g_WorldToShadowMapUVDepth;
    float4   g_LightDirection;
};

struct PlanePSInput
{
    float4 Pos          : SV_POSITION;
    float3 ShadowMapPos : SHADOW_MAP_POS;
    float  NdotL        : N_DOT_L;
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
    PSIn.ShadowMapPos = ShadowMapPos.xyz / ShadowMapPos.w;
    PSIn.NdotL        = saturate(dot(float3(0.0, 1.0, 0.0), -g_LightDirection.xyz));
}