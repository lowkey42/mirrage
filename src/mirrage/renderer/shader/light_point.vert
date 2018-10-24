#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "global_uniforms.glsl"

layout(location = 0) in vec3 position;

layout(push_constant) uniform Per_model_uniforms {
	mat4 transform;
	vec4 light_color;
	vec4 light_data;  // R=src_radius, GBA=position
	vec4 light_data2; // R=shadowmapID
} model_uniforms;

out gl_PerVertex {
	vec4 gl_Position;
};

void main() {
	gl_Position = model_uniforms.transform * vec4(position, 1.0f);
}
