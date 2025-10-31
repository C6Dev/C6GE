// Lighting + transform constants shared between VS/PS
// Declare light structs and limits to keep identical layout with PS
#define MAX_POINT_LIGHTS 8
#define MAX_SPOT_LIGHTS  8
struct PointLight
{
    float3 Position; float Range;
    float3 Color;    float Intensity;
};
struct SpotLight
{
    float3 Position; float Range;
    float3 Color;    float Intensity;
    float3 Direction; float SpotCos; // cosine of cutoff angle
};

cbuffer Constants
{
    // Object/world transforms
    float4x4 g_World;
    float4x4 g_WorldViewProj;
    float4x4 g_NormalTranform;

    // Directional light (xyz = direction, w = intensity)
    float4   g_DirLight;
    // Ambient color (rgb), a unused
    float4   g_Ambient;

    // Light counts and padding
    uint     g_NumPointLights;
    uint     g_NumSpotLights;
    float2   _pad0;

    // Fixed-size light arrays to keep cbuffer size identical to PS
    PointLight g_PointLights[MAX_POINT_LIGHTS];
    SpotLight  g_SpotLights[MAX_SPOT_LIGHTS];
};

struct VSInput
{
    float3 Pos    : ATTRIB0;
    float3 Normal : ATTRIB1;
    float2 UV     : ATTRIB2;
};

struct PSInput 
{ 
    float4 Pos    : SV_POSITION; 
    float3 Normal : NORMAL;
    float2 UV     : TEXCOORD0;
    float3 WorldPos : TEXCOORD1;
};

void main(in  VSInput VSIn,
          out PSInput PSIn) 
{
    float4 wpos = mul(float4(VSIn.Pos, 1.0), g_World);
    PSIn.Pos      = mul(wpos, g_WorldViewProj);
    PSIn.WorldPos = wpos.xyz;
    PSIn.Normal   = mul(float4(VSIn.Normal, 0.0), g_NormalTranform).xyz;
    PSIn.UV       = VSIn.UV;
}