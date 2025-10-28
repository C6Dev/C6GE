cbuffer RTConstants
{
    float4x4 g_InvViewProj;
    float2   g_ViewportSize; // (width, height)
    float    g_PlaneY;       // -2.0
    float    g_PlaneExtent;  // 5.0
    float3   g_LightDir;     // same as CPU m_LightDirection (pointing from light to scene)
    float    g_ShadowStrength; // e.g., 0.5
    float    g_LightAngularRadius; // radians, e.g., 0.05 (~2.9 deg)
    float    g_SoftShadowSampleCount; // N samples (as float, cast to int)
    float2   g_Padding_RT;
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

// Hash function for per-pixel randomness
float Hash12(float2 p)
{
    float3 p3  = frac(float3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return frac((p3.x + p3.y) * p3.z);
}

// Build an orthonormal basis (U,V) around a normalized vector N
void BuildBasis(float3 N, out float3 U, out float3 V)
{
    float3 Up = abs(N.y) < 0.99 ? float3(0,1,0) : float3(1,0,0);
    U = normalize(cross(Up, N));
    V = cross(N, U);
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
    float3 reflectionColor = 0.0.xxx;

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
            // Soft shadow: sample multiple rays within an angular radius around L
            float3 N = float3(0.0, 1.0, 0.0);
            float3 L = normalize(-g_LightDir); // incoming light direction used in raster path

            // Build basis around L for cone sampling
            float3 U, V;
            BuildBasis(L, U, V);
            float tanTheta = tan(saturate(g_LightAngularRadius));

            // Determine sample count
            int Nsamples = max(1, (int)round(g_SoftShadowSampleCount));
            float seed = Hash12(pix);
            float occludedCount = 0.0;
            // Golden-angle spiral sampling on unit disk
            const float GOLDEN_ANGLE = 2.39996323;
            for (int i = 0; i < Nsamples; ++i)
            {
                float t = (i + 0.5) / Nsamples;
                float r = sqrt(t);
                float a = (i * GOLDEN_ANGLE) + seed * 6.2831853; // rotate per pixel
                float2 d = r * float2(cos(a), sin(a));

                // Offset light direction within cone using tangent plane U,V
                float3 dL = normalize(L + (U * d.x + V * d.y) * tanTheta);

                RayDesc shadow;
                shadow.Origin = hitPos + N * 1e-3;
                shadow.Direction = dL;
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
                occludedCount += occluded ? 1.0 : 0.0;
            }
            float occ = occludedCount / Nsamples;
            shadowFactor = lerp(1.0, g_ShadowStrength, occ);

            // Simple planar reflection: reflect view ray off the plane and test scene hit
            float3 R = reflect(dir, N);
            RayDesc refl;
            refl.Origin = hitPos + N * 1e-3;
            refl.Direction = normalize(R);
            refl.TMin = 0.0;
            refl.TMax = 1e20;

            RayQuery<RAY_FLAG_NONE> rqRefl;
            rqRefl.TraceRayInline(
                g_TLAS,
                RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH,
                0xFF,
                refl
            );
            while (rqRefl.Proceed()) { }
            bool reflHit = (rqRefl.CommittedStatus() == COMMITTED_TRIANGLE_HIT);
            if (reflHit)
            {
                // Constant-tint reflection with simple Fresnel-ish view dependence
                float fres = pow(saturate(1.0 - abs(dot(dir, N))), 5.0);
                float strength = lerp(0.15, 0.45, fres);
                reflectionColor = strength * float3(0.7, 0.75, 0.8);
            }
        }
    }

    // Pack reflection in RGB and shadow factor in Alpha
    g_RTOutputUAV[DTid.xy] = float4(reflectionColor, shadowFactor);
}
