#include "bgfx_shader.sh"

SAMPLER2D(s_texColor, 0);
uniform vec4 u_tonemap;

void main()
{
	vec4 color = texture2D(s_texColor, v_texcoord0);
	
	// Simple blur - in a full implementation you'd use a proper blur kernel
	gl_FragColor = color;
}
