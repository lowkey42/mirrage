#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "global_uniforms.glsl"
#include "normal_encoding.glsl"
#include "poisson.glsl"
#include "random.glsl"
#include "brdf.glsl"


layout(location = 0) in Vertex_data {
	vec2 tex_coords;
} vertex_out;

layout(location = 0) out vec4 out_color;

layout(set=1, binding = 1) uniform sampler2D accum_sampler;
layout(set=1, binding = 2) uniform sampler2D revealage_sampler;

void main() {
	float revealage = texture(revealage_sampler, vertex_out.tex_coords, 0).r;

	if (revealage == 1.0) {
		// Save the blending and color texture fetch cost
		discard;
	}

	vec4 accum     = texture(accum_sampler, vertex_out.tex_coords, 0);

	// Suppress overflow
	vec4 abs_accum = abs(accum);
	if (isinf(max(abs_accum.x, max(abs_accum.y, max(abs_accum.z, abs_accum.w))))) {
		accum.rgb = vec3(accum.a);
	}

	vec3 averageColor = accum.rgb / max(accum.a, 0.00001);

	// dst' =  (accum.rgb / accum.a) * (1 - revealage) + dst * revealage
	out_color = vec4(averageColor, 1.0 - revealage);
}
