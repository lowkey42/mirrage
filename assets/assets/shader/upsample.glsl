#ifndef UPSAMPLE_INCLUDED
#define UPSAMPLE_INCLUDED

#include "poisson.glsl"
#include "random.glsl"


vec4 upsampled_smooth(sampler2D color_sampler, float lod, vec2 tex_coords, float scale) {
	if(lod > pcs.arguments.y)
		return vec4(0,0,0,0);

	vec2 texture_size = textureSize(color_sampler, int(lod+0.5));
	vec2 rand = PDnrand2(vec4(tex_coords, lod, 0.0));

	vec4 c = vec4(0,0,0,0);
	float weight = 0.0;
	for(int i=0; i<8; i++) {
		vec2 offset = vec2(Poisson8[i].x*rand.x - Poisson8[i].y*rand.y, Poisson8[i].x*rand.y + Poisson8[i].y*rand.x);

		vec2 uv = tex_coords + offset/texture_size * scale;

		float w = 1.0 - clamp(dot(offset,offset), 0.0, 0.9);
		c += textureLod(color_sampler, uv, lod) * w;
		weight += w;
	}

	return c / weight;
}

vec4 upsampled_max(sampler2D color_sampler, float lod, vec2 tex_coords, float scale) {
	if(lod > pcs.arguments.y)
		return vec4(0,0,0,0);

	vec2 texture_size = textureSize(color_sampler, int(lod+0.5));

	vec4 c = vec4(0,0,0,0);
	for(int i=0; i<8; i++) {
		vec2 uv = tex_coords + Poisson8[i]/texture_size * scale;

		c = max(c, textureLod(color_sampler, uv, lod));
	}

	return c;
}

vec4 upsampled_result(sampler2D color_sampler, int lod, vec2 tex_coords, float scale) {
	if(lod > pcs.arguments.y)
		return vec4(0,0,0,0);

	vec2 texture_size = textureSize(color_sampler, lod);
	ivec2 center = ivec2(texture_size*tex_coords);

	float depth = texelFetch(depth_sampler, center*2, max(0, lod-1)).r;


	const vec2 offsets[4] = vec2[](
		vec2(-0.5, -0.5),
		vec2( 0.5, -0.5),
		vec2( 0.5,  0.5),
		vec2(-0.5,  0.5)
	);


	float depths[4];

	for(int i=0; i<4; i++) {
		ivec2 uv = ivec2(center + offsets[i]);
		depths[i] = texelFetch(depth_sampler, uv, lod).r;
	}


	if(abs(depths[0] - depth) > 0.05 ||
	   abs(depths[1] - depth) > 0.05 ||
	   abs(depths[2] - depth) > 0.05 ||
	   abs(depths[3] - depth) > 0.05) {
		// edge detected
		vec4 c = vec4(0,0,0,0);
		float weight_sum = 0.0;

		for(int i=0; i<4; i++) {
			ivec2 uv = ivec2(center + offsets[i]);
			float weight = 1.0 / clamp(abs(depth-depths[i]), 0.1, 4.0);
			c += weight * texelFetch(color_sampler, uv, lod);
			weight_sum += weight;
		}
		return c / weight_sum;

	} else {
		return upsampled_smooth(color_sampler, lod, tex_coords, scale);
	}
}

vec4 upsampled_prev_result(sampler2D color_sampler, int current_lod, vec2 tex_coords) {
	return upsampled_result(color_sampler, current_lod + 1, tex_coords, 4.0);
}

#endif
