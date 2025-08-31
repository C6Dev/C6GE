#include "bgfx_shader.sh"

SAMPLER2D(s_texColor, 0);
SAMPLER2D(s_texLum, 1);
SAMPLER2D(s_texBlur, 2);
uniform vec4 u_tonemap;

void main()
{
	vec4 color = texture2D(s_texColor, v_texcoord0);
	vec4 lum = texture2D(s_texLum, v_texcoord0);
	vec4 blur = texture2D(s_texBlur, v_texcoord0);
	
	float middleGray = u_tonemap.x;
	float whiteSqr = u_tonemap.y;
	float threshold = u_tonemap.z;
	
	// Simple Reinhard tonemapping
	vec3 tonemapped = color.rgb / (color.rgb + vec3(1.0, 1.0, 1.0));
	
	// Add bloom
	vec3 bloom = blur.rgb * 0.5;
	tonemapped += bloom;
	
	gl_FragColor = vec4(tonemapped, color.a);
}
