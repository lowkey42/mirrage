#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "global_uniforms.glsl"


layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;

layout(location = 0) out vec3 out_color;

layout(push_constant) uniform Push_constants {
	mat4 projection;
} pcs;

out gl_PerVertex {
	vec4 gl_Position;
};


void main() {
	gl_Position = pcs.projection * vec4(position, 1.0);
	out_color = color;
}
