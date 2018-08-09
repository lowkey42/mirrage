#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "global_uniforms.glsl"


layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 tex_coords;
layout(location = 3) in ivec4 bone_ids;
layout(location = 4) in vec4 bone_weights;

layout(location = 0) out vec3 out_view_pos;
layout(location = 1) out vec3 out_normal;
layout(location = 2) out vec2 out_tex_coords;

layout(set=2, binding = 0, std140) uniform Bone_uniforms {
	mat3x4 offset[64];
} bones;

layout(push_constant) uniform Per_model_uniforms {
	mat4 model_to_view;
	vec4 light_color;
	vec4 options;
} model_uniforms;

out gl_PerVertex {
	vec4 gl_Position;
};


vec3 transform_position(vec3 p, mat3x4 dq) {
	p *= dq[2].xyz;

	return p +
        2 * cross(dq[0].xyz, cross(dq[0].xyz, p) + dq[0].w*p) +
        2 * (dq[0].w * dq[1].xyz - dq[1].w * dq[0].xyz +
            cross(dq[0].xyz, dq[1].xyz));
}

vec3 transform_normal(vec3 n, mat3x4 dq) {
	return n + 2.0 * cross(dq[0].xyz, cross(dq[0].xyz, n) + dq[0].w * n);
}

void main() {
	float unused_weight = 1.0 - dot(bone_weights, vec4(1.0));
	mat3x4 identity_dqs = mat3x4(vec4(1,0,0,0), vec4(0,0,0,0), vec4(1,1,1,1));

	mat3x4 bone = bones.offset[bone_ids[0]] * bone_weights[0]
	        + bones.offset[bone_ids[1]] * bone_weights[1]
			+ bones.offset[bone_ids[2]] * bone_weights[2]
			+ bones.offset[bone_ids[3]] * bone_weights[3]
	        + identity_dqs * unused_weight;
	float dq_len = length(bone[0]);
	bone[0] /= dq_len;
	bone[1] /= dq_len;

	vec3 p = transform_position(position, bone);
	vec3 n = transform_normal  (normal,   bone);

	//vec3 p = position;
	//vec3 n = normal;

	vec4 view_pos = model_uniforms.model_to_view * vec4(p, 1.0);

	out_view_pos = view_pos.xyz / view_pos.w;
	out_normal  = (model_uniforms.model_to_view * vec4(n, 0.0)).xyz;
	out_tex_coords = tex_coords;

	gl_Position = global_uniforms.proj_mat * view_pos;
}
