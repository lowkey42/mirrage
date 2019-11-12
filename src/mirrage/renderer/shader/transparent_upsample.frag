#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "global_uniforms.glsl"

layout(location = 0) in Vertex_data {
	vec2 tex_coords;
} vertex_out;

layout(location = 0) out vec4 accum_out;
layout(location = 1) out vec4 revealage_out;

layout(set=1, binding = 1) uniform sampler2D accum_sampler;
layout(set=1, binding = 2) uniform sampler2D revealage_sampler;
layout(set=1, binding = 3) uniform sampler2D depth_base_sampler;
layout(set=1, binding = 4) uniform sampler2D depth_target_sampler;

// calculate a weighting factor based on the normal differences x and the deviation
vec4 filter_depth(vec4 x) {
	return vec4(greaterThanEqual(x+vec4(0.01), vec4(0)));
}
vec4 weight_depth(vec4 x, float dev) {
	return clamp(exp(-x*x / (2*dev*dev)), vec4(0.8), vec4(1.0));
}
vec4 to_linear_depth(vec4 d) {
	vec4 z_n = 2.0 * d - 1.0;
	vec4 z_e = 2.0 * global_uniforms.proj_planes.x * global_uniforms.proj_planes.y / (global_uniforms.proj_planes.y + global_uniforms.proj_planes.x - z_n * (global_uniforms.proj_planes.y - global_uniforms.proj_planes.x));
	return z_e;
}

void main() {
	float depth = texelFetch(depth_target_sampler, ivec2(textureSize(depth_target_sampler, 0)*vertex_out.tex_coords), 0).r;

	// calculate uv coordinates
	vec2 tex_size = textureSize(depth_base_sampler, 0);
	vec2 uv_00 = vertex_out.tex_coords + vec2(-1,-1) / tex_size;
	vec2 uv_10 = vertex_out.tex_coords + vec2( 1,-1) / tex_size;
	vec2 uv_11 = vertex_out.tex_coords + vec2( 1, 1) / tex_size;
	vec2 uv_01 = vertex_out.tex_coords + vec2(-1, 1) / tex_size;

	// calculate the maximum depth deviation based on the distance, to reduce bluring
	//   near the camera where it's most noticable
	float depth_dev = 0.01;
	depth = to_linear_depth(depth);

	// initialize the per-pixel weights with gaussian weights
	vec4 weight_00 = vec4(2,4,2,1);
	vec4 weight_10 = vec4(2,1,1,1);
	vec4 weight_11 = vec4(1,1,1,2);
	vec4 weight_01 = vec4(1,1,2,1);

	vec4 depth_diff_00 = to_linear_depth(textureGather(depth_base_sampler, uv_00, 0)) - depth;
	vec4 depth_diff_10 = to_linear_depth(textureGather(depth_base_sampler, uv_10, 0)) - depth;
	vec4 depth_diff_11 = to_linear_depth(textureGather(depth_base_sampler, uv_11, 0)) - depth;
	vec4 depth_diff_01 = to_linear_depth(textureGather(depth_base_sampler, uv_01, 0)) - depth;

	// sample low-res depth and modulate the weights based on their difference to the high-res depth
	vec4 rev_weight_00 = weight_00 * filter_depth(depth_diff_00);
	vec4 rev_weight_10 = weight_10 * filter_depth(depth_diff_10);
	vec4 rev_weight_11 = weight_11 * filter_depth(depth_diff_11);
	vec4 rev_weight_01 = weight_01 * filter_depth(depth_diff_01);

	weight_00 *= weight_depth(depth_diff_00, depth_dev);
	weight_10 *= weight_depth(depth_diff_10, depth_dev);
	weight_11 *= weight_depth(depth_diff_11, depth_dev);
	weight_01 *= weight_depth(depth_diff_01, depth_dev);

	float weight_sum = dot(weight_00, vec4(1))
	        + dot(weight_10, vec4(1))
			+ dot(weight_11, vec4(1))
			+ dot(weight_01, vec4(1));

	float rev_weight_sum = dot(rev_weight_00, vec4(1))
	        + dot(rev_weight_10, vec4(1))
			+ dot(rev_weight_11, vec4(1))
			+ dot(rev_weight_01, vec4(1));

	if(weight_sum<0.000001 || rev_weight_sum<0.000001) {
		revealage_out = textureLod(revealage_sampler, vertex_out.tex_coords, 0);
		accum_out = textureLod(accum_sampler, vertex_out.tex_coords, 0);

	} else {
		float revealage = 1.0 - dot(vec4(1),
						(vec4(1) - textureGather(revealage_sampler, uv_00, 0)) * rev_weight_00
					  + (vec4(1) - textureGather(revealage_sampler, uv_10, 0)) * rev_weight_10
					  + (vec4(1) - textureGather(revealage_sampler, uv_11, 0)) * rev_weight_11
					  + (vec4(1) - textureGather(revealage_sampler, uv_01, 0)) * rev_weight_01) / rev_weight_sum;

		float accum_r = dot(vec4(1),
						textureGather(accum_sampler, uv_00, 0) * weight_00
					  + textureGather(accum_sampler, uv_10, 0) * weight_10
					  + textureGather(accum_sampler, uv_11, 0) * weight_11
					  + textureGather(accum_sampler, uv_01, 0) * weight_01) / weight_sum;

		float accum_g = dot(vec4(1),
						textureGather(accum_sampler, uv_00, 1) * weight_00
					  + textureGather(accum_sampler, uv_10, 1) * weight_10
					  + textureGather(accum_sampler, uv_11, 1) * weight_11
					  + textureGather(accum_sampler, uv_01, 1) * weight_01) / weight_sum;

		float accum_b = dot(vec4(1),
						textureGather(accum_sampler, uv_00, 2) * weight_00
					  + textureGather(accum_sampler, uv_10, 2) * weight_10
					  + textureGather(accum_sampler, uv_11, 2) * weight_11
					  + textureGather(accum_sampler, uv_01, 2) * weight_01) / weight_sum;

		float accum_a = dot(vec4(1),
						textureGather(accum_sampler, uv_00, 3) * weight_00
					  + textureGather(accum_sampler, uv_10, 3) * weight_10
					  + textureGather(accum_sampler, uv_11, 3) * weight_11
					  + textureGather(accum_sampler, uv_01, 3) * weight_01) / weight_sum;

		revealage_out = vec4(revealage, 0, 0, 1);
		accum_out = vec4(accum_r, accum_g, accum_b, accum_a);
	}
}
