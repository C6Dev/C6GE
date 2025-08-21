$input a_position
$output v_color

#include "../bgfx_shader.sh"

void main()
{
    // Use vertex ID to determine position and color
    int vertexId = gl_VertexID % 3;
    
    vec3 pos;
    if (vertexId == 0) {
        pos = vec3( 0.0,  0.5, 0.0);  // Top vertex
        v_color = vec4(1.0, 0.0, 0.0, 1.0);  // Red
    } else if (vertexId == 1) {
        pos = vec3(-0.5, -0.5, 0.0);  // Bottom left
        v_color = vec4(0.0, 1.0, 0.0, 1.0);  // Green
    } else {
        pos = vec3( 0.5, -0.5, 0.0);  // Bottom right
        v_color = vec4(0.0, 0.0, 1.0, 1.0);  // Blue
    }
    
    // Apply model-view-projection matrix
    gl_Position = mul(u_modelViewProj, vec4(pos, 1.0));
}
