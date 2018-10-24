#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "normal_encoding.glsl"
#include "color_conversion.glsl"


layout(location = 0) in Vertex_data {
	vec2 tex_coords;
} vertex_out;

layout(location = 0) out vec4 out_color;

layout(set=1, binding = 0) uniform sampler2D bloom_sampler;

layout(push_constant) uniform Settings {
	vec4 options;
} pcs;

void main() {
	out_color = vec4(textureLod(bloom_sampler, vertex_out.tex_coords, pcs.options.x).rgb, 1);
	out_color *= 0.04;
}
