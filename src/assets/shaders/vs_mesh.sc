/*
 * Copyright 2011-2025 Branimir Karadzic. All rights reserved.
 * License: https://github.com/bkaradzic/bgfx/blob/master/LICENSE
 */

#include "../common/common.sh"

uniform vec4 u_time;

void main()
{
	vec3 pos = a_position;
	vec3 normal = a_normal.xyz*2.0 - 1.0;

	// Remove displacement - render mesh normally
	// pos = pos + normal*displacement*vec3(0.06, 0.06, 0.06);

	gl_Position = mul(u_modelViewProj, vec4(pos, 1.0) );
	v_pos = gl_Position.xyz;
	v_view = mul(u_modelView, vec4(pos, 1.0) ).xyz;

	v_normal = mul(u_modelView, vec4(normal, 0.0) ).xyz;

	// Simple white color instead of displacement-based color
	v_color0 = vec4(1.0, 1.0, 1.0, 1.0);
}
