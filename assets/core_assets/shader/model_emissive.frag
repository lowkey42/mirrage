#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "global_uniforms.glsl"
#include "normal_encoding.glsl"

layout(location = 0) in vec3 view_pos;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 tex_coords;


layout(location = 0) out vec4 depth_out;
layout(location = 1) out vec4 albedo_mat_id;
layout(location = 2) out vec4 mat_data_out;
layout(location = 3) out vec4 color_out;
layout(location = 4) out vec4 color_diffuse_out;

layout(set=1, binding = 0) uniform sampler2D albedo_sampler;
layout(set=1, binding = 1) uniform sampler2D mat_data_sampler;
layout(set=1, binding = 2) uniform sampler2D mat_data2_sampler;

layout(push_constant) uniform Per_model_uniforms {
	mat4 model;
	vec4 light_color;
	vec4 material_properties;
} model_uniforms;

const float PI = 3.14159265359;


vec3 decode_tangent_normal(vec2 tn);
vec3 tangent_space_to_world(vec3 N);

void main() {
	vec4 albedo = texture(albedo_sampler, tex_coords);

	if(albedo.a < 0.1)
		discard;

	vec4 mat_data = texture(mat_data_sampler, tex_coords);

	vec3  N    = tangent_space_to_world(decode_tangent_normal(mat_data.rg));
	N = normalize(normal);

	float roughness = mat_data.b;
	float metallic  = mat_data.a;

	roughness = mix(0.01, 0.99, roughness*roughness);

	albedo.rgb *= model_uniforms.material_properties.rgb;

	depth_out     = vec4(-view_pos.z / global_uniforms.proj_planes.y, 0,0,1);
	albedo_mat_id = vec4(albedo.rgb, 1.0);
	mat_data_out      = vec4(encode_normal(N), 1, 0);
	color_out     = vec4(albedo.rgb * texture(mat_data2_sampler, tex_coords).r * model_uniforms.material_properties.a, 1.0);
	color_diffuse_out = color_out;
}


vec3 decode_tangent_normal(vec2 tn) {
	if(dot(tn,tn)<0.00001)
		return vec3(0,0,1);

	vec3 N = vec3(tn*2-1, 0);
	N.z = sqrt(1 - dot(N.xy, N.xy));
	return N;
}

vec3 tangent_space_to_world(vec3 N) {
	vec3 VN = normalize(normal);

	// calculate tangent
	vec3 p_dx = dFdx(view_pos);
	vec3 p_dy = dFdy(view_pos);

	vec2 tc_dx = dFdx(tex_coords);
	vec2 tc_dy = dFdy(tex_coords);

	vec3 p_dy_N = cross(p_dy, VN);
	vec3 p_dx_N = cross(VN, p_dx);

	vec3 T = p_dy_N * tc_dx.x + p_dx_N * tc_dy.x;
	vec3 B = p_dy_N * tc_dx.y + p_dx_N * tc_dy.y;

	float inv_max = inversesqrt(max(dot(T,T), dot(B,B)));
	mat3 TBN = mat3(T*inv_max, B*inv_max, VN);
	return normalize(TBN * N);
}

