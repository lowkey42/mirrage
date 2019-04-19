#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "global_uniforms.glsl"
#include "normal_encoding.glsl"

layout(location = 0) in vec3 view_pos;
layout(location = 1) in vec4 ndc_pos;


layout(location = 0) out vec4 albedo_mat_id_out;
layout(location = 1) out vec4 mat_data_out;
layout(location = 2) out vec4 color_out;
layout(location = 3) out vec4 color_diffuse_out;

layout(set=1, binding = 0) uniform sampler2D albedo_sampler;
layout(set=1, binding = 1) uniform sampler2D normal_sampler;
layout(set=1, binding = 2) uniform sampler2D brdf_sampler;
layout(set=1, binding = 3) uniform sampler2D emission_sampler;

layout(input_attachment_index = 0, set=2, binding = 0) uniform subpassInput depth_sampler;


layout(push_constant) uniform Per_model_uniforms {
	mat4 model_to_view;
	mat4 view_to_model;
} model_uniforms;

const float PI = 3.14159265359;


vec3 decode_tangent_normal(vec2 tn);
vec3 tangent_space_to_world(vec3 N, vec3 p_dx, vec3 p_dy, vec2 tc_dx, vec2 tc_dy);

void main() {
	vec2 screen_tex_coords = ndc_pos.xy/ndc_pos.w /2.0 +0.5;

	vec4 clip_rect = vec4(unpackSnorm2x16(floatBitsToUint(model_uniforms.model_to_view[0][3])),
	                      unpackSnorm2x16(floatBitsToUint(model_uniforms.model_to_view[1][3])))*10.0;
	vec4 color = vec4(unpackUnorm2x16(floatBitsToUint(model_uniforms.model_to_view[2][3])),
	                  unpackUnorm2x16(floatBitsToUint(model_uniforms.model_to_view[3][3])));

	vec4 emissive_color = vec4(model_uniforms.view_to_model[0][3], model_uniforms.view_to_model[1][3],
	                           model_uniforms.view_to_model[2][3], model_uniforms.view_to_model[3][3]);

	mat4 view_to_model = model_uniforms.view_to_model;
	view_to_model[0][3] = 0;
	view_to_model[1][3] = 0;
	view_to_model[2][3] = 0;
	view_to_model[3][3] = 1;

	float depth = subpassLoad(depth_sampler).r;
	vec3 position = position_from_ldepth(screen_tex_coords, depth);
	vec4 decal_pos = view_to_model * vec4(position, 1.0);
	decal_pos /= decal_pos.w;
	decal_pos.xyz += 0.5;

	vec2 tex_coords = clip_rect.xy + decal_pos.xy*clip_rect.zw;
	tex_coords.y = 1.0-tex_coords.y;

	vec3 p_dx = dFdx(position);
	vec3 p_dy = dFdy(position);

	vec2 tc_dx = dFdx(tex_coords);
	vec2 tc_dy = dFdy(tex_coords);

	if(decal_pos.x<0 || decal_pos.y<0 || decal_pos.z<0 || decal_pos.x>1 || decal_pos.y>1 || decal_pos.z>1) {
		discard;
	}

	vec4 albedo = texture(albedo_sampler, tex_coords);
	if(albedo.a<0.05)
		discard;

	albedo *= color;
	albedo.rgb *= albedo.a;

	vec3 N    = tangent_space_to_world(decode_tangent_normal(texture(normal_sampler, tex_coords).rg), p_dx, p_dy, tc_dx, tc_dy);
	vec4 brdf = texture(brdf_sampler, tex_coords);
	float roughness = brdf.r;
	float metallic  = brdf.g;
	roughness = mix(0.01, 0.99, roughness*roughness);

	float emissive_power = texture(emission_sampler, tex_coords).r;

	albedo_mat_id_out = albedo;
	mat_data_out      = vec4(encode_normal(N), roughness, metallic);
	color_out         = vec4(albedo.rgb * emissive_color.rgb
	                         * emissive_power * emissive_color.a * albedo.a, albedo.a);
	color_diffuse_out = color_out;
}

vec3 decode_tangent_normal(vec2 tn) {
	if(dot(tn,tn)<0.00001)
		return vec3(0,0,1);

	vec3 N = vec3(tn*2-1, 0);
	N.z = sqrt(1 - dot(N.xy, N.xy));
	return N;
}

vec3 tangent_space_to_world(vec3 N, vec3 p_dx, vec3 p_dy, vec2 tc_dx, vec2 tc_dy) {
	// calculate tangent
	vec3 VN = -normalize(cross(p_dx, p_dy));

	vec3 p_dy_N = cross(p_dy, VN);
	vec3 p_dx_N = cross(VN, p_dx);

	vec3 T = p_dy_N * tc_dx.x + p_dx_N * tc_dy.x;
	vec3 B = p_dy_N * tc_dx.y + p_dx_N * tc_dy.y;

	float inv_max = inversesqrt(max(dot(T,T), dot(B,B)));
	mat3 TBN = mat3(T*inv_max, B*inv_max, VN);
	return normalize(TBN * N);
}
