#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "global_uniforms.glsl"


layout(location = 0) out vec3 out_view_pos;
layout(location = 1) out vec2 out_tex_coords;

layout(push_constant) uniform Per_model_uniforms {
	vec4 position;
	vec4 size; // xy, z=screen_space, w=fixed_screen_size
	vec4 clip_rect;
	vec4 color;
	vec4 emissive_color;
} model_uniforms;

out gl_PerVertex {
	vec4 gl_Position;
};

const vec2 vertex_positions[4] = vec2[](
	vec2(0, 0),
	vec2(1, 0),
	vec2(0, 1),
	vec2(1, 1)
);

void main() {

	vec2 p = vertex_positions[gl_VertexIndex];
	vec2 offset = (p-0.5)*model_uniforms.size.xy;

	if(model_uniforms.position.w>0.5)
		offset = vec2(offset.x, 0) + offset.y*vec2(global_uniforms.view_mat[1][0], global_uniforms.view_mat[1][1]);

	vec4 view_pos = vec4(model_uniforms.position.xyz, 1.0);
	if(model_uniforms.size.w<0.5)
		view_pos.xy += offset;

	vec4 ndc_pos = global_uniforms.proj_mat * view_pos;
	if(model_uniforms.size.z>=0.5)
		ndc_pos = view_pos;

	if(model_uniforms.size.w>=0.5) {
		ndc_pos.xy += vec2(offset.x, -offset.y)*ndc_pos.w;
	}

	gl_Position = ndc_pos;
	out_view_pos = view_pos.xyz;
	out_tex_coords = model_uniforms.clip_rect.xy + p * model_uniforms.clip_rect.zw;
	out_tex_coords.y = 1-out_tex_coords.y;
}
