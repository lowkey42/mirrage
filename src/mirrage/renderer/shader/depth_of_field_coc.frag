#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "global_uniforms.glsl"

layout(location = 0) in Vertex_data {
	vec2 tex_coords;
} vertex_out;

layout(location = 0) out vec4 out_color;

layout(set=1, binding = 0) uniform sampler2D color_sampler;
layout(set=1, binding = 1) uniform sampler2D depth_sampler;

layout (constant_id = 0) const float MAX_RADIUS = 15;
layout (constant_id = 1) const float RAD_SCALE = 0.5;

layout(push_constant) uniform Push_constants {
	// focus_distance, range, radius
	vec4 parameters;
	mat4 depth_reprojection;
} pcs;


float calc_coc(float dist) {
	const float focus_distance = pcs.parameters[0];
	const float range = pcs.parameters[1];
	const float radius = pcs.parameters[2];

	return MAX_RADIUS * radius * clamp((dist - focus_distance) / dist / range, -1.0, 1.0);
}

void main() {
	vec4 red   = textureGather(color_sampler, vertex_out.tex_coords, 0);
	vec4 green = textureGather(color_sampler, vertex_out.tex_coords, 1);
	vec4 blue  = textureGather(color_sampler, vertex_out.tex_coords, 2);

	vec3 color = vec3(
		max(red[0], max(red[1], max(red[2], red[3]))),
		max(green[0], max(green[1], max(green[2], green[3]))),
		max(blue[0], max(blue[1], max(blue[2], blue[3])))
	);

	vec4 depth_uv = pcs.depth_reprojection * vec4(vertex_out.tex_coords*2-1, 0.5, 1);
	depth_uv.xy = (depth_uv.xy/depth_uv.w)*0.5+0.5;

	vec4 depth  = textureGather(depth_sampler, depth_uv.xy, 0);
	vec4 dist   = depth * global_uniforms.proj_planes[1];

	float coc = calc_coc(dot(vec4(1.0), dist)/4.0);

	out_color = vec4(color, coc);
}
