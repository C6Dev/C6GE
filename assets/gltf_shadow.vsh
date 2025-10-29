cbuffer Constants
{
    float4x4 g_WorldViewProj;
};

struct VSInput
{
    float3 Pos : ATTRIB0;
};

struct VSOutput
{
    float4 Pos : SV_Position;
};

VSOutput main(VSInput In)
{
    VSOutput Out;
    Out.Pos = mul(float4(In.Pos, 1.0), g_WorldViewProj);
    return Out;
}
