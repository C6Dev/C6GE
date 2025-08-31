#include "bgfx_shader.sh"

SAMPLER2D(s_texColor, 0);

void main()
{
	vec4 color = texture2D(s_texColor, v_texcoord0);
	float lum = dot(color.rgb, vec3(0.2126, 0.7152, 0.0722) );
	gl_FragColor = vec4(lum, lum, lum, 1.0);
}
