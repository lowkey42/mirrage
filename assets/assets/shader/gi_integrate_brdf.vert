#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "global_uniforms.glsl"

layout(location = 0) out Vertex_data {
	vec2 tex_coords;
} vertex_out;

out gl_PerVertex {
	vec4 gl_Position;
};

void main() {
	vertex_out.tex_coords = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	gl_Position = vec4(vertex_out.tex_coords * 2.0f + -1.0f, 0.0f, 1.0f);
}
