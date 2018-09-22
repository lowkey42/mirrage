#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "color_conversion.glsl"

layout(location = 0) in Vertex_data {
	vec2 tex_coords;
} vertex_out;

layout(location = 0) out vec4 out_color;

layout(set=1, binding = 0) uniform sampler2D color_sampler;
layout(set=1, binding = 1) uniform sampler2D histogram_adjustment_sampler;

layout (constant_id = 0) const float HISTOGRAM_MIN = 0;
layout (constant_id = 1) const float HISTOGRAM_MAX = 0;
layout (constant_id = 2) const int HISTOGRAM_SLOTS = 256;


float calc_histogram_index_fp(float luminance) {
	luminance = (luminance-HISTOGRAM_MIN) / (HISTOGRAM_MAX-HISTOGRAM_MIN);
	return clamp(luminance, 0, 1);
}

void main() {
	float min_mesoptic = 0.00031622776f;
	float max_mesoptic = 3.16227766017f;
	vec3  cie_white    = vec3(0.950456,1.,1.08906);


	vec3 color = textureLod(color_sampler, vertex_out.tex_coords, 0).rgb*10000.0;

	vec3 cie_color = rgb2cie(color);
	float lum = cie_color.y;

	// scotopic simulation
	float scotopic_lum = min(lum, max_mesoptic);
	float alpha = clamp((scotopic_lum - min_mesoptic) / (max_mesoptic-min_mesoptic), 0.0, 1.0);
	cie_color = mix(cie_white * scotopic_lum, cie_color, alpha);

	// histogram adjustment / tone mapping
	float idx = calc_histogram_index_fp(log(lum));
	cie_color *= texture(histogram_adjustment_sampler, vec2(idx, 0)).r;

	out_color = vec4(cie2rgb(cie_color), 1.0);
}
