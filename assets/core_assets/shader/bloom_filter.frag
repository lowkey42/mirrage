#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "normal_encoding.glsl"
#include "color_conversion.glsl"


layout(location = 0) in Vertex_data {
	vec2 tex_coords;
} vertex_out;

layout(location = 0) out vec4 out_color;

layout(set=1, binding = 0) uniform sampler2D color_sampler;
layout(set=1, binding = 1) uniform sampler2D avg_log_luminance_sampler;

layout(push_constant) uniform Settings {
	vec4 options;
} pcs;


vec3 expose(vec3 color, float threshold) {
	float avg_luminance = max(exp(texture(avg_log_luminance_sampler, vec2(0.5, 0.5)).r), 0.0001);

	float key = 1.03f - (2.0f / (2 + log(avg_luminance + 1)/log(10)));
	float exposure = clamp(key/avg_luminance, 0.1, 6.0) + 0.5;

	if(pcs.options.z>0) {
		exposure = pcs.options.z;
	}

	exposure = log2(exposure);
	exposure -= threshold;

	return color * exp2(exposure);
}

void main() {
	float lower_bound = max(exp(texelFetch(avg_log_luminance_sampler, ivec2(0, 0), 0).r), 0.0001);
	float upper_bound = max(exp(texelFetch(avg_log_luminance_sampler, ivec2(1, 0), 0).r), 0.0001);

	vec3 color = texture(color_sampler, vertex_out.tex_coords).rgb;
	float lum = rgb2cie(color*10).y;

	//color *= smoothstep(lower_bound, upper_bound, lum)*0.95+0.05;

	color *= 0.087 * 10.0;

	out_color = vec4(color, 1.0);
}
