struct VSOut { float4 Pos : SV_Position; float2 UV : TEXCOORD0; };

VSOut main(uint vid : SV_VertexID)
{
    // Full screen triangle without vertex buffer
    float2 pos;
    if (vid == 0) pos = float2(-1.0, -1.0);
    else if (vid == 1) pos = float2(-1.0,  3.0);
    else               pos = float2( 3.0, -1.0);

    VSOut o;
    o.Pos = float4(pos, 0.0, 1.0);
    // Map clip-space to UV; adjust for the full screen triangle
    o.UV = 0.5 * (pos + 1.0);
    return o;
}
