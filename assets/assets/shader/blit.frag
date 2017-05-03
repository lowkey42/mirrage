#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "normal_encoding.glsl"


layout(location = 0) in Vertex_data {
	vec2 tex_coords;
} vertex_out;

layout(location = 0) out vec4 out_color;

layout(set=1, binding = 0) uniform sampler2D albedo_sampler;

layout(push_constant) uniform Settings {
	vec4 options;
} settings;


vec3 heji_dawson(vec3 color) {
	vec3 X = max(vec3(0.0), color-0.004);
	vec3 mapped = (X*(6.2*X+.5))/(X*(6.2*X+1.7)+0.06);
	return mapped * mapped;
}

vec3 tone_mapping(vec3 color) {
	float exposure = settings.options.r;
	color *= exposure;
	color = heji_dawson(color);

	return color;
}

void main() {
	out_color = vec4(tone_mapping(texture(albedo_sampler, vertex_out.tex_coords).rgb), 1.0);
}
