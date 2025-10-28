struct GSInput
{
    float4 Pos    : SV_POSITION;
    float3 Normal : NORMAL;
    float2 UV     : TEXCOORD0;
};

struct GSOutput
{
    float4 Pos    : SV_POSITION;
    float3 Normal : NORMAL;
    float2 UV     : TEXCOORD0;
};

[maxvertexcount(3)]
void main(triangle GSInput input[3], inout TriangleStream<GSOutput> OutputStream)
{
    GSOutput output;
    
    for(int i = 0; i < 3; ++i)
    {
        output.Pos    = input[i].Pos;
        output.Normal = input[i].Normal;
        output.UV     = input[i].UV;
        OutputStream.Append(output);
    }
}