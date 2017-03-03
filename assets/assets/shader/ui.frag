#version auto
precision mediump float;

in vec2 uv_frag;
in vec4 color_frag;

out vec4 out_color;

uniform sampler2D tex;


void main() {
	vec4 c = texture(tex, uv_frag) * color_frag;

	if(c.a>0.01)
		out_color = c;
	else
		discard;
}
