#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable


layout(location = 0) in Vertex_data {
	vec2 tex_coords;
} vertex_out;

layout(location = 0) out vec4 out_color;

layout(set=1, binding = 0) uniform sampler2D color_sampler;

float luminance(vec3 c) {
	vec3 f = vec3(0.299,0.587,0.114);
	return max(dot(c, f), 0.0);
}

void main() {
	vec3 color = textureLod(color_sampler, vertex_out.tex_coords, 0).rgb;

	out_color = vec4(log(luminance(color) + 0.000001), 0, 0, 1.0);
}
