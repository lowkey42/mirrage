#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "normal_encoding.glsl"


layout(location = 0) in Vertex_data {
	vec2 tex_coords;
} vertex_out;

layout(location = 0) out vec4 out_color;

layout(set=1, binding = 0) uniform sampler2D color_sampler[2];

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

float luminance(vec3 c) {
	vec3 f = vec3(0.299,0.587,0.114);
	return sqrt(c.r*c.r*f.r + c.g*c.g*f.g + c.b*c.b*f.b);
}

vec3 resolve_txaa() {
	vec3 curr = texture(color_sampler[0], vertex_out.tex_coords).rgb; // TODO: unjitter
	vec3 prev  = texture(color_sampler[1], vertex_out.tex_coords).rgb;
	
	float lum_curr = luminance(curr);
	float lum_prev = luminance(prev);
	
	float lum_diff = abs(lum_curr - lum_prev) / max(lum_curr, max(lum_prev, 0.2));
	float weight = 1.0 - lum_diff;
	float t = mix(0.4, 0.02, weight*weight);
	
	return mix(curr,prev, 0.5);//t);
}

void main() {
	out_color = vec4(tone_mapping(resolve_txaa()), 1.0);
}
