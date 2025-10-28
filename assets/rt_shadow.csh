cbuffer RTConstants
{
    float4x4 g_InvViewProj;
    float2   g_ViewportSize; // (width, height)
    float    g_PlaneY;       // -2.0
    float    g_PlaneExtent;  // 5.0
    float3   g_LightDir;     // same as CPU m_LightDirection (pointing from light to scene)
    float    g_ShadowStrength; // e.g., 0.5
};

RWTexture2D<float4> g_RTOutputUAV;
RaytracingAccelerationStructure g_TLAS;

// Unproject NDC to world using inverse view-projection
float3 Unproject(float2 pixel, float ndcZ)
{
    // Map pixel coords (origin top-left) to NDC (origin center)
    float2 ndcXY;
    ndcXY.x = (pixel.x / g_ViewportSize.x) * 2.0f - 1.0f;
    ndcXY.y = (pixel.y / g_ViewportSize.y) * 2.0f - 1.0f;
    float4 p     = mul(float4(ndcXY, ndcZ, 1.0), g_InvViewProj);
    return p.xyz / p.w;
}

[numthreads(8,8,1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    if (DTid.x >= (uint)g_ViewportSize.x || DTid.y >= (uint)g_ViewportSize.y)
        return;

    float2 pix = float2(DTid.xy) + 0.5;
    float3 worldNear = Unproject(pix, 0.0);
    float3 worldFar  = Unproject(pix, 1.0);
    float3 dir       = normalize(worldFar - worldNear);
    float3 orig      = worldNear;

    // Ray-plane intersection with plane y = g_PlaneY
    float denom = dir.y;
    bool denomOK = abs(denom) > 1e-6;
    float t_plane = denomOK ? (g_PlaneY - orig.y) / denom : -1.0;
    bool planeHit = denomOK && t_plane > 0.0;
    float3 hitPos = orig + dir * t_plane;

    // Check within plane extent
    planeHit = planeHit && abs(hitPos.x) <= g_PlaneExtent && abs(hitPos.z) <= g_PlaneExtent;

    float shadowFactor = 1.0;

    if (planeHit)
    {
        // Primary visibility test: if any geometry is closer than the plane, we should not modulate (cube in front)
        RayDesc primary;
        primary.Origin = orig;
        primary.Direction = dir;
        primary.TMin = 0.0;
        primary.TMax = t_plane - 1e-4; // only search up to the plane

        RayQuery<RAY_FLAG_NONE> rqPrimary;
        rqPrimary.TraceRayInline(
            g_TLAS,
            RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH,
            0xFF,
            primary
        );
        bool blockedByGeometry = false;
        while (rqPrimary.Proceed()) { }
        blockedByGeometry = (rqPrimary.CommittedStatus() == COMMITTED_TRIANGLE_HIT);

        if (!blockedByGeometry)
        {
            // Shadow ray from plane hit towards the light direction
            float3 N = float3(0.0, 1.0, 0.0);
            float3 L = normalize(-g_LightDir); // incoming light direction used in raster path

            RayDesc shadow;
            shadow.Origin = hitPos + N * 1e-3;
            shadow.Direction = L;
            shadow.TMin = 0.0;
            shadow.TMax = 1e20;

            RayQuery<RAY_FLAG_NONE> rq;
            rq.TraceRayInline(
                g_TLAS,
                RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH,
                0xFF,
                shadow
            );
            while (rq.Proceed()) { }
            bool occluded = (rq.CommittedStatus() == COMMITTED_TRIANGLE_HIT);
            shadowFactor = occluded ? g_ShadowStrength : 1.0;
        }
    }

    g_RTOutputUAV[DTid.xy] = float4(shadowFactor.xxx, 1.0);
}
