#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 position;
layout(location = 1) in vec2 uv;
layout(location = 2) in vec4 color;

layout(location = 0) out vec2 uv_frag;
layout(location = 1) out vec4 color_frag;

layout(push_constant) uniform PushConstants {
    mat4 vp;
} uniforms;


out gl_PerVertex {
	vec4 gl_Position;
};


void main() {
	gl_Position = uniforms.vp * vec4(position, 0, 1);
	uv_frag = uv;
	color_frag = color;
}
