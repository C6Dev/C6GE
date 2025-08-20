#include "bgfx_shader.sh"

void main()
{
    // Triangle vertices in world space (positioned in front of camera)
    vec3 positions[3];
    positions[0] = vec3( 0.0,  0.5, -2.0);  // top
    positions[1] = vec3(-0.5, -0.5, -2.0);  // bottom left
    positions[2] = vec3( 0.5, -0.5, -2.0);  // bottom right
    
    // Transform by view-projection matrix
    gl_Position = mul(u_viewProj, vec4(positions[gl_VertexID], 1.0));
}
