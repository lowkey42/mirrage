#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "color_conversion.glsl"

layout(location = 0) in Vertex_data {
	vec2 tex_coords;
} vertex_out;

layout(location = 0) out vec4 out_color;

layout(set=1, binding = 0) uniform sampler2D color_sampler;

void main() {
	vec3 color = textureLod(color_sampler, vertex_out.tex_coords, 0).rgb;

	vec3 cie_color = rgb2cie(color);
	float scotopic_lum = cie_color.y * (1.33*(1+(cie_color.y+cie_color.z)/cie_color.x)-1.68);
	float min_mesoptic = 0.00031622776f/10/2;
	float max_mesoptic = 3.16227766017f/10/2;
	float alpha = clamp((scotopic_lum-min_mesoptic) / (max_mesoptic-min_mesoptic), 0.25, 1);
	out_color = vec4(mix(vec3(scotopic_lum), color, alpha), 1.0);
}
