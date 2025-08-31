#include "bgfx_shader.sh"

SAMPLER2D(s_texColor, 0);
uniform vec4 u_offset[16];

void main()
{
	vec4 color = vec4(0.0, 0.0, 0.0, 0.0);
	
	for (int i = 0; i < 16; i++)
	{
		color += texture2D(s_texColor, v_texcoord0 + u_offset[i].xy);
	}
	
	gl_FragColor = color / 16.0;
}
