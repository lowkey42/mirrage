#ifndef GLOBAL_UNIFORMS_INCLUDED
#define GLOBAL_UNIFORMS_INCLUDED

layout(std140, set=0, binding = 0) uniform Global_uniforms {
	mat4 view_proj_mat;
	mat4 view_mat;
	mat4 proj_mat;
	mat4 inv_view_mat;
	mat4 inv_proj_mat;
	vec4 eye_pos;
	vec4 proj_planes; //< near, far, fov horizontal, fov vertical
	vec4 time; //< time, sin(time), delta_time, frame-number % 20
	vec4 proj_info;
} global_uniforms;

vec3 position_from_ldepth(vec2 uv, float z) {
	z *= -global_uniforms.proj_planes.y;
	return vec3((uv.xy * global_uniforms.proj_info.xy + global_uniforms.proj_info.zw), 1) * z;
}

float to_linear_depth(float d) {
	float z_n = 2.0 * d - 1.0;
	float z_e = 2.0 * global_uniforms.proj_planes.x * global_uniforms.proj_planes.y / (global_uniforms.proj_planes.y + global_uniforms.proj_planes.x - z_n * (global_uniforms.proj_planes.y - global_uniforms.proj_planes.x));
	return z_e;
}

#endif
