#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "global_uniforms.glsl"

layout(location = 0) out Vertex_data {
	vec2 tex_coords;
	vec3 view_ray;
	flat vec3 corner_view_rays[4];
} vertex_out;

out gl_PerVertex {
	vec4 gl_Position;
};


vec3 construct_view_ray(vec2 uv) {
	vec4 vr = global_uniforms.inv_proj_mat * vec4(uv * 2.0 + -1.0, 1.0, 1.0);
	return vr.xyz / vr.w;
}

void main() {
	vertex_out.tex_coords = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	gl_Position = vec4(vertex_out.tex_coords * 2.0f + -1.0f, 0.0f, 1.0f);

	vertex_out.view_ray = construct_view_ray(vertex_out.tex_coords);

	vertex_out.corner_view_rays[0] = construct_view_ray(vec2(0,0));
	vertex_out.corner_view_rays[1] = construct_view_ray(vec2(1,0));
	vertex_out.corner_view_rays[2] = construct_view_ray(vec2(0,1));
	vertex_out.corner_view_rays[3] = construct_view_ray(vec2(1,1));
}
