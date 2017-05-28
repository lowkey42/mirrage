#ifndef UPSAMPLE_INCLUDED
#define UPSAMPLE_INCLUDED

#include "poisson.glsl"
#include "random.glsl"


vec4 upsampled_result(sampler2D color_sampler, float lod, vec2 tex_coords, float scale) {
	if(lod > pcs.arguments.y)
		return vec4(0,0,0,0);

	vec2 texture_size = textureSize(color_sampler, int(lod));
	ivec2 center = ivec2(texture_size*tex_coords);

	float depth = texelFetch(depth_sampler, center*2, max(0, int(lod)-1)).r;
/*
	const ivec2 offsets[4] = ivec2[](
		ivec2(-1, -1),
		ivec2( 1, -1),
		ivec2( 1,  1),
		ivec2(-1,  1)
	);

	float depths[4];

	ivec2 top_uv;
	float top_depth = 999.0;

	for(int i=0; i<4; i++) {
		ivec2 uv = center + offsets[i];
		depths[i] = texelFetch(depth_sampler, uv, int(lod)).r;

		float dd = abs(depth-depths[i]);
		if(dd <= top_depth) {
			top_depth = dd;
			top_uv = uv;
		}
	}

	if(abs(depths[0] - depth) > 0.1 ||
	   abs(depths[1] - depth) > 0.1 ||
	   abs(depths[2] - depth) > 0.1 ||
	   abs(depths[3] - depth) > 0.1) {
		// edge detected
		return texelFetch(result_sampler, top_uv, int(lod)).rgb;

	} else {*/
		vec4 c = vec4(0,0,0,0);
		float weight_sum = 0.0;
		for(int i=0; i<8; i++) {
			vec2 uv = tex_coords + Poisson8[i]/texture_size * scale;
			float d = textureLod(depth_sampler, uv, lod).r;

			// TODO: causes noise
			float weight = 1.0;// / (0.0001 + abs(depth-d));

			c += weight * textureLod(color_sampler, uv, lod);
			weight_sum += weight;
		}

		return c / weight_sum;
//	}
}

vec4 upsampled_prev_result(sampler2D color_sampler, float current_lod, vec2 tex_coords) {
	return upsampled_result(color_sampler, current_lod + 0.9999999, tex_coords, 4.0);
}

#endif
