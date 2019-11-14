#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable


layout(location = 0) in Vertex_data {
	vec2 tex_coords;
} vertex_out;

layout(location = 0) out vec4 out_color;

layout(set=1, binding = 0) uniform sampler2D gi_sampler;
layout(set=1, binding = 1) uniform sampler2D color_sampler;


void main() {
	vec3 gi = textureLod(gi_sampler, vertex_out.tex_coords, 0).rgb;
	vec3 c  = textureLod(color_sampler, vertex_out.tex_coords, 0).rgb;

	c*= smoothstep(0.02, 0.1, length(c));
	float cl = length(c);
	if(cl>4.0)
		c *= 4.0 / cl;

	out_color = vec4(gi+c, 1.0);
}
