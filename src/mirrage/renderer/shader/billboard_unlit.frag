#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "global_uniforms.glsl"
#include "normal_encoding.glsl"

layout(location = 0) in vec3 view_pos;
layout(location = 1) in vec2 tex_coords;

layout(location = 0) out vec4 color_out;

layout(set=1, binding = 0) uniform sampler2D albedo_sampler;
layout(set=1, binding = 1) uniform sampler2D normal_sampler;
layout(set=1, binding = 2) uniform sampler2D brdf_sampler;
layout(set=1, binding = 3) uniform sampler2D emission_sampler;

layout(push_constant) uniform Per_model_uniforms {
	vec4 position;
	vec4 size; // xy, z=screen_space, w=fixed_screen_size
	vec4 clip_rect;
	vec4 color;
	vec4 emissive_color;
} model_uniforms;


void main() {
	vec4 albedo = texture(albedo_sampler, tex_coords);
	albedo *= model_uniforms.color;

	if(albedo.a<0.05)
		discard;

	color_out = vec4(albedo.rgb * albedo.a, 0.0);
}
