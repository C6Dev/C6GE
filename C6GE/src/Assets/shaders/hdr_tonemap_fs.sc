$input v_uv

/*
 * Simple Reinhard tone mapping with exposure
 */

#include "../bgfx_shader.sh"

SAMPLER2D(s_hdrColor, 0);
uniform vec4 u_exposure; // x = exposure

void main()
{
	vec3 hdr = texture2D(s_hdrColor, v_uv).rgb;
	vec3 mapped = 1.0 - exp(-hdr * u_exposure.x);
	gl_FragColor = vec4(mapped, 1.0);
}


