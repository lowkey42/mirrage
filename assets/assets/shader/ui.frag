#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 uv_frag;
layout(location = 1) in vec4 color_frag;

layout(location = 0) out vec4 out_color;

layout(set=0, binding = 0) uniform sampler2D tex;


void main() {
	vec4 c = texture(tex, uv_frag) * color_frag;

	if(c.a>0.01)
		out_color = vec4(c.rgb*c.a, c.a);
	else
		discard;
}
