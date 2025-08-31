$input v_texcoord0, v_normal, v_position

#include "../common/common.sh"

SAMPLER2D(s_texColor, 0);
uniform vec4 u_hdrParams; // x: exposure, y: gamma, z: unused, w: unused

void main()
{
    vec4 color = texture2D(s_texColor, v_texcoord0);
    
    // Apply HDR parameters
    float exposure = u_hdrParams.x;      // Middle Gray (exposure)
    float gamma = u_hdrParams.y;         // Gamma correction
    float whitePoint = u_hdrParams.z;    // White Point
    float threshold = u_hdrParams.w;     // Threshold (bright pass)
    
    // Apply exposure
    vec3 exposed = color.rgb * exposure;
    
    // Apply threshold (bright pass) - only keep bright pixels
    vec3 brightPass = max(exposed - threshold, vec3(0.0, 0.0, 0.0));
    
    // Apply Reinhard tonemapping with white point adjustment
    vec3 tonemapped = exposed / (exposed + vec3(1.0, 1.0, 1.0));
    
    // Apply white point adjustment
    if (whitePoint > 0.0) {
        tonemapped = tonemapped * (1.0 + tonemapped / whitePoint) / (1.0 + tonemapped);
    }
    
    // Add bloom effect from bright pass
    vec3 bloom = brightPass * 0.5; // Bloom intensity
    tonemapped = tonemapped + bloom;
    
    // Apply gamma correction
    tonemapped = pow(tonemapped, vec3(1.0 / gamma));
    
    gl_FragColor = vec4(tonemapped, color.a);
}
