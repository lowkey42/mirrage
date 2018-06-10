#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "color_conversion.glsl"

layout(location = 0) in Vertex_data {
	vec2 tex_coords;
} vertex_out;

layout(location = 0) out vec4 out_color;

layout(set=1, binding = 0) uniform sampler2D color_sampler;


void main() {
	ivec2 input_size = textureSize(color_sampler, 0);
	int cx = int(vertex_out.tex_coords.x*input_size.x);
	int cy = int(vertex_out.tex_coords.y*input_size.y);

	float weights = 0.0f;
	vec3  sum = vec3(0);
/*
	for(int y = max(0,cy-input_size.y/10);  y<min(input_size.y, cy+input_size.y/10); y++) {
		for(int x = max(0,cx-input_size.x/10);  x<min(input_size.x, cx+input_size.x/10); x++) {
			if(x!=cx || y!=cy) {
				float dx = x-cx;
				float dy = y-cy;
				float angle = sqrt(dx*dx + dy*dy) * 0.01745329251;

				float weight = abs(cos(angle) / (angle*angle));

				weights += weight;
				sum += weight * texelFetch(color_sampler, ivec2(x,y), 0).rgb;
			}
		}
	}
*/
	out_color = weights>0.000001 ? vec4(0.087 * sum/weights, 1.0) : vec4(0,0,0,1);
}
