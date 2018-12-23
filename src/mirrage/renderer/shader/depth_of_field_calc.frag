#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "global_uniforms.glsl"
#include "random.glsl"


layout(location = 0) in Vertex_data {
	vec2 tex_coords;
} vertex_out;

layout(location = 0) out vec4 out_color;

layout(set=1, binding = 0) uniform sampler2D color_sampler;

layout (constant_id = 0) const float MAX_RADIUS = 15;
layout (constant_id = 1) const float RAD_SCALE = 0.8;

layout(push_constant) uniform Push_constants {
	// focus_distance, range, radius
	vec4 parameters;
} pcs;


void main() {
	vec4  center      = textureLod(color_sampler, vertex_out.tex_coords, 0);
	float center_coc  = center.a;
	float center_coc2 = abs(2.0 * center.a);
	vec3  color       = center.rgb;
	float weight_sum  = 1.0;
	float coc_sum     = abs(center_coc);

	vec2 texel_size = 1.0 / textureSize(color_sampler, 0);

	float radius = RAD_SCALE;
	for (float ang = 0; radius<MAX_RADIUS; ang += 2.39996323) {
		vec2 tc = vertex_out.tex_coords + vec2(cos(ang), sin(ang)) * texel_size * radius;

		vec4 sample_data = textureLod(color_sampler, tc, 0);
		float coc = abs(sample_data.a);

		if(sample_data.a > center_coc)
			sample_data.a = min(coc, center_coc2);

		float m = smoothstep(radius-0.5, radius+0.5, coc);
		color += mix(color/weight_sum, sample_data.rgb, m);
		coc_sum += mix(coc_sum/weight_sum, coc, m);
		weight_sum += 1.0;
		radius += RAD_SCALE/radius;
	}


	out_color = vec4(color/weight_sum, coc_sum/weight_sum);
}
