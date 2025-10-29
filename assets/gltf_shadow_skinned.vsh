cbuffer Constants
{
    float4x4 g_WorldViewProj;
};

cbuffer ShadowSkin
{
    // Maximum joints supported in this simple shadow pass
    float4x4 g_JointMatrices[256];
    int      g_JointCount;
    float3   _pad0; // align to 16 bytes
};

struct VSInput
{
    float3 Pos     : ATTRIB0;
    float4 Joints0 : ATTRIB4; // Indices into joint matrix array
    float4 Weights0: ATTRIB5; // Corresponding weights
};

struct VSOutput
{
    float4 Pos : SV_Position;
};

float4x4 GetJoint(int idx)
{
    // Clamp to valid range
    idx = clamp(idx, 0, g_JointCount - 1);
    return g_JointMatrices[idx];
}

VSOutput main(VSInput In)
{
    VSOutput Out;

    // Skin deform the position using the first 4 joints
    float4 p = float4(In.Pos, 1.0);

    // glTF JOINTS_0 are typically unsigned, but come in as floats from our loader
    int4 ji = int4(In.Joints0 + 0.5);
    float4 w = In.Weights0;

    float4 sp = mul(p, GetJoint(ji.x)) * w.x +
                mul(p, GetJoint(ji.y)) * w.y +
                mul(p, GetJoint(ji.z)) * w.z +
                mul(p, GetJoint(ji.w)) * w.w;

    Out.Pos = mul(sp, g_WorldViewProj);
    return Out;
}
