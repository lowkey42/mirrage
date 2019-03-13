#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "global_uniforms.glsl"
#include "normal_encoding.glsl"
#include "particle/data_structures.glsl"

layout(early_fragment_tests) in;

layout(location = 0) in vec3 view_pos;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 tex_coords;
layout(location = 3) in vec4 out_particle_color;

layout(location = 0) out vec4 accum_out;
layout(location = 1) out vec4 revealage_out;

layout(set=1, binding = 0) uniform sampler2D albedo_sampler;
layout(set=1, binding = 1) uniform sampler2D normal_sampler;
layout(set=1, binding = 2) uniform sampler2D brdf_sampler;
layout(set=1, binding = 3) uniform sampler2D emission_sampler;

layout(std140, set=2, binding = 0) readonly buffer Particle_type_config {
	PARTICLE_TYPE_CONFIG
} particle_config;

layout(push_constant) uniform Per_model_uniforms {
	mat4 model;
	vec4 light_color;
	vec4 options;
} model_uniforms;

const float PI = 3.14159265359;


vec3 decode_tangent_normal(vec2 tn);
vec3 tangent_space_to_world(vec3 N);

void main() {
	vec4 albedo = texture(albedo_sampler, tex_coords);
	albedo *= out_particle_color;

	if(albedo.a<0.001) {
		discard;
	}

	vec3  N    = tangent_space_to_world(decode_tangent_normal(texture(normal_sampler, tex_coords).rg));

	vec4 brdf = texture(brdf_sampler, tex_coords);
	float roughness = brdf.r;
	float metallic  = brdf.g;
	roughness = mix(0.01, 0.99, roughness*roughness);

	float emissive_power = texture(emission_sampler, tex_coords).r;

	vec4 result_color = vec4(0,0,0,0);
	vec3 transmit     = vec3(1.0 - albedo.a);
	result_color.rgb += albedo.rgb * emissive_power * albedo.a;

	// TODO: lighting
	result_color += vec4(albedo.rgb * albedo.a, albedo.a);

	/* Modulate the net coverage for composition by the transmission. This does not affect the color channels of the
	  transparent surface because the caller's BSDF model should have already taken into account if transmission modulates
	  reflection. This model doesn't handled colored transmission, so it averages the color channels. See

	  McGuire and Enderton, Colored Stochastic Shadow Maps, ACM I3D, February 2011
	  http://graphics.cs.williams.edu/papers/CSSM/

	  for a full explanation and derivation.*/

	result_color.a *= 1.0 - clamp((transmit.r + transmit.g + transmit.b) * (1.0 / 3.0), 0, 1);

	/* You may need to adjust the w function if you have a very large or very small view volume; see the paper and
	   presentation slides at http://jcgt.org/published/0002/02/09/ */
	// Intermediate terms to be cubed
	float a = min(1.0, result_color.a) * 8.0 + 0.01;
	float b = -gl_FragCoord.z * 0.95 + 1.0;

	/* If your scene has a lot of content very close to the far plane,
	   then include this line (one rsqrt instruction):
	   b /= sqrt(1e4 * abs(csZ)); */
	float w       = clamp(a * a * a * 1e8 * b * b * b, 1e-2, 3e2);
	accum_out     = result_color * w;
	revealage_out = vec4(result_color.a, 0,0,0);
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

