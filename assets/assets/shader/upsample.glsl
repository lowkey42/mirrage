#ifndef UPSAMPLE_INCLUDED
#define UPSAMPLE_INCLUDED

#include "poisson.glsl"
#include "random.glsl"


float weight_offset(float x) {
	float b = 0;
	float c = 2*2 + 2*2;
	return 0.5 * exp(- (x-b)*(x-b) / (2*c*c));
}
float weight_depth(float x) {
	float b = 0;
	float c = 0.01f;
	return 0.1 * exp(- (x-b)*(x-b) / (2*c*c));
}

vec4 upsampled_result(sampler2D color_sampler, int depth_lod, int color_lod, vec2 tex_coords) {
	if(color_lod > pcs.arguments.y)
		return vec4(0,0,0,0);
	
	vec2 texture_size = textureSize(color_sampler, color_lod);
	float depth = texelFetch(depth_sampler, ivec2(texture_size*tex_coords*2), depth_lod).r;
	
	float weight_sum = 0;
	vec4 color = vec4(0,0,0,0);
	
	for(float x=-2; x<=2; x++) {
		for(float y=-2; y<=2; y++) {
			vec2 offset = vec2(x,y);
			
			float d = texelFetch(depth_sampler, ivec2((texture_size*tex_coords + offset)*2), depth_lod).r;
			
			float weight = weight_offset(dot(offset,offset)) * weight_depth(depth-d);
			
			color += weight * texelFetch(color_sampler, ivec2(texture_size*tex_coords + offset), color_lod);
			weight_sum += weight;
		}
	}
	
	return color / weight_sum;
}

#endif
