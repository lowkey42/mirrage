#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "global_uniforms.glsl"


layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 tex_coords;

layout(location = 0) out vec3 out_view_pos;
layout(location = 1) out vec3 out_normal;
layout(location = 2) out vec2 out_tex_coords;

layout(push_constant) uniform Per_model_uniforms {
	mat4 model_to_view;
	vec4 light_color;
	vec4 options;
} model_uniforms;

out gl_PerVertex {
	vec4 gl_Position;
};


void main() {
	vec4 view_pos = model_uniforms.model_to_view * vec4(position, 1.0);

	out_view_pos = view_pos.xyz / view_pos.w;
	out_normal  = (model_uniforms.model_to_view *  vec4(normal, 0.0)).xyz;
	out_tex_coords = tex_coords;

	gl_Position = global_uniforms.proj_mat * view_pos;
}
