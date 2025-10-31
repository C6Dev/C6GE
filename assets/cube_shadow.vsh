cbuffer Constants
{
    float4x4 g_WorldViewProj;
};

// Vertex shader takes two inputs: vertex position and color.
// By convention, Diligent Engine expects vertex shader inputs to be 
// labeled 'ATTRIBn', where n is the attribute number.
struct VSInput
{
    float3 Pos    : ATTRIB0;
    float3 Normal : ATTRIB1; // unused
    float2 UV     : ATTRIB2; // unused
};

struct PSInput 
{
    float4 Pos   : SV_POSITION;
};

// Shadow pass vertex shader - only outputs position for depth rendering
void main(in  VSInput VSIn,
          out PSInput PSIn)
{
    PSIn.Pos = mul(float4(VSIn.Pos, 1.0), g_WorldViewProj);
}