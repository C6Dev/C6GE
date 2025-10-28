// Fills the RT output UAV with a simple color gradient to validate the path.
RWTexture2D<float4> g_RTOutputUAV;

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint w, h;
    g_RTOutputUAV.GetDimensions(w, h);
    if (DTid.x >= w || DTid.y >= h)
        return;

    float2 uv = (float2(DTid.xy) + 0.5) / float2(w, h);
    // RG channels form a gradient, alpha ~0.35 for additive blend that doesn't overpower scene
    float4 color = float4(uv.x, 0.2, uv.y, 0.35);
    g_RTOutputUAV[DTid.xy] = color;
}
