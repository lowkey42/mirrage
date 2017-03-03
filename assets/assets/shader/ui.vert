#version auto
precision mediump float;

in vec2 position;
in vec2 uv;
in vec4 color;

out vec2 uv_frag;
out vec4 color_frag;

uniform mat4 vp;

void main() {
	gl_Position = vp * vec4(position, 0, 1);
	uv_frag = uv;
	color_frag = color;
}
