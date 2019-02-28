#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "global_uniforms.glsl"


layout(location = 0) out vec3 out_view_pos;
layout(location = 1) out vec4 out_ndc_pos;

layout(push_constant) uniform Per_model_uniforms {
	mat4 model_to_view;
	mat4 view_to_model;
} model_uniforms;

out gl_PerVertex {
	vec4 gl_Position;
};

const vec3 vertex_positions[14] = vec3[](
	vec3(-1.f, 1.f, 1.f),
	vec3(1.f, 1.f, 1.f),
	vec3(-1.f, -1.f, 1.f),
	vec3(1.f, -1.f, 1.f),
	vec3(1.f, -1.f, -1.f),
	vec3(1.f, 1.f, 1.f),
	vec3(1.f, 1.f, -1.f),
	vec3(-1.f, 1.f, 1.f),
	vec3(-1.f, 1.f, -1.f),
	vec3(-1.f, -1.f, 1.f),
	vec3(-1.f, -1.f, -1.f),
	vec3(1.f, -1.f, -1.f),
	vec3(-1.f, 1.f, -1.f),
	vec3(1.f, 1.f, -1.f)
);

void main() {
	vec3 p = vertex_positions[gl_VertexIndex];

	mat4 model_to_view = model_uniforms.model_to_view;
	model_to_view[0][3] = 0;
	model_to_view[1][3] = 0;
	model_to_view[2][3] = 0;
	model_to_view[3][3] = 1;

	vec4 view_pos = model_to_view * vec4(p*0.5, 1.0);

	out_view_pos = view_pos.xyz / view_pos.w;
	vec4 ndc_pos = global_uniforms.proj_mat * view_pos;
	gl_Position = ndc_pos;
	out_ndc_pos = ndc_pos;
}
