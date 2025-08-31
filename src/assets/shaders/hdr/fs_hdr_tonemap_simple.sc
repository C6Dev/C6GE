$input v_color0, v_texcoord0

#include "bgfx_shader.sh"

SAMPLER2D(s_texColor, 0);
uniform vec4 u_tonemap;

void main()
{
	vec4 color = texture2D(s_texColor, v_texcoord0);
	
	float middleGray = u_tonemap.x;
	float whiteSqr = u_tonemap.y;
	float threshold = u_tonemap.z;
	
	// Apply HDR tonemapping (Reinhard operator)
	vec3 tonemapped = color.rgb / (color.rgb + vec3(1.0, 1.0, 1.0));
	
	// Apply exposure adjustment
	tonemapped *= middleGray;
	
	// Apply white point adjustment
	tonemapped = tonemapped * (1.0 + tonemapped / whiteSqr) / (1.0 + tonemapped);
	
	gl_FragColor = vec4(tonemapped, color.a);
}
