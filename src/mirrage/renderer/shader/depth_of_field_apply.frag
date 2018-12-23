#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "global_uniforms.glsl"

layout(location = 0) in Vertex_data {
	vec2 tex_coords;
} vertex_out;

layout(location = 0) out vec4 out_color;

layout(set=1, binding = 0) uniform sampler2D color_sampler;
layout(set=1, binding = 1) uniform sampler2D dof_sampler;

void main() {
	vec4 dof   = textureLod(dof_sampler, vertex_out.tex_coords, 0);
	vec3 color = textureLod(color_sampler, vertex_out.tex_coords, 0).rgb;
	out_color  = vec4(mix(color, dof.rgb, smoothstep(0.1, 2.0, dof.a)), 1.0);
}
