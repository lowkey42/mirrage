#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "normal_encoding.glsl"
#include "color_conversion.glsl"


layout(location = 0) in Vertex_data {
	vec2 tex_coords;
	vec3 view_ray;
	flat vec3 corner_view_rays[4];
} vertex_out;

layout(location = 0) out vec4 out_color;

layout(set=1, binding = 0) uniform sampler2D color_sampler;
layout(set=1, binding = 1) uniform sampler2D avg_log_luminance_sampler;

layout(push_constant) uniform Settings {
	vec4 options;
} pcs;


vec3 expose(vec3 color, float threshold) {
	float avg_luminance = max(exp(texture(avg_log_luminance_sampler, vec2(0.5, 0.5)).r), 0.001);

	float key = 1.03f - (2.0f / (2 + log(avg_luminance + 1)/log(10)));
	float exposure = log2(clamp(key/avg_luminance, 0.1, 20.0));
	exposure -= threshold;

	return max(color * exp2(exposure), vec3(0));
}

void main() {
	out_color = vec4(expose(texture(color_sampler, vertex_out.tex_coords).rgb, pcs.options.x), 1.0);
}
