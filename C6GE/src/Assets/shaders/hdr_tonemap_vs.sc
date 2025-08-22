$input a_position
$output v_uv

/*
 * Minimal fullscreen triangle VS: pass UV from NDC position
 */

#include "../bgfx_shader.sh"

void main()
{
	gl_Position = vec4(a_position, 1.0);
	// Map NDC [-1,1] to UV [0,1]
	v_uv = gl_Position.xy * 0.5 + 0.5;
}


