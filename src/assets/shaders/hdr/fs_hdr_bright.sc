#include "bgfx_shader.sh"

SAMPLER2D(s_texColor, 0);
SAMPLER2D(s_texLum, 1);
uniform vec4 u_tonemap;

void main()
{
	vec4 color = texture2D(s_texColor, v_texcoord0);
	vec4 lum = texture2D(s_texLum, v_texcoord0);
	
	float threshold = u_tonemap.z;
	float offset = 0.0;
	
	vec3 bright = max(color.rgb - threshold, vec3(0.0, 0.0, 0.0) );
	float brightLum = dot(bright, vec3(0.2126, 0.7152, 0.0722) );
	
	gl_FragColor = vec4(bright, brightLum);
}
