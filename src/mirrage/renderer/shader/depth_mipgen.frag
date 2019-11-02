#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location = 0) in Vertex_data {
	vec2 tex_coords;
} vertex_out;

layout(set=1, binding = 0) uniform sampler2D depth_sampler;

void main() {
	gl_FragDepth = textureLod(depth_sampler, vertex_out.tex_coords, 0).r;
}
