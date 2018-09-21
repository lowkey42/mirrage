#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "global_uniforms.glsl"
#include "normal_encoding.glsl"

layout(early_fragment_tests) in;

layout(location = 0) in vec3 color;

layout(location = 0) out vec4 color_out;

void main() {
	color_out = vec4(color, 1.0);
}
