#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "global_uniforms.glsl"

layout(location = 0) in Vertex_data {
	vec2 tex_coords;
	vec3 view_ray;
	flat vec3 corner_view_rays[4];
} vertex_out;

layout(location = 0) out vec4 out_color;

layout(set=1, binding = 0) uniform sampler2D current_lum_sampler;
layout(set=1, binding = 1) uniform sampler2D prev_lum_sampler;

layout(push_constant) uniform Push_constants {
	vec4 parameters;
} pcs;

void main() {
	float current = exp(texture(current_lum_sampler, vertex_out.tex_coords).r);
	float prev = exp(texture(prev_lum_sampler, vertex_out.tex_coords).r);

	const float tau = current>prev ? pcs.parameters.x : pcs.parameters.y;
	float lum = prev + (current - prev) * (1 - exp(-global_uniforms.time.z * tau));

	out_color = vec4(log(lum), 0, 0, 1.0);
}
