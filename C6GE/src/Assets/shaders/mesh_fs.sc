$input v_normal, v_texcoord0

/*
 * Copyright 2011-2025 Branimir Karadzic. All rights reserved.
 * License: https://github.com/bkaradzic/bgfx/blob/master/LICENSE
 */

#include "../bgfx_shader.sh"

SAMPLER2D(s_diffuse, 0);
SAMPLER2D(s_roughness, 1);

void main()
{
	vec3 albedo = texture2D(s_diffuse, v_texcoord0).rgb;
	float rough = texture2D(s_roughness, v_texcoord0).r;

	vec3 N = normalize(v_normal);
	// View-space light pointing towards camera (positive Z in bgfx view space)
	vec3 L = normalize(vec3(0.0, 0.0, 1.0));
	float ndotl = max(dot(N, L), 0.0);

	float ambient = 0.2;
	float lambert = ambient + (1.0 - ambient) * ndotl;

	float shininess = mix(64.0, 4.0, clamp(rough, 0.0, 1.0));
	float spec = pow(max(ndotl, 0.0), shininess) * (1.0 - rough) * 0.25;

	vec3 color = albedo * lambert + vec3_splat(spec);
	gl_FragColor = vec4(color, 1.0);
}
