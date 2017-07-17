#ifndef UPSAMPLE_INCLUDED
#define UPSAMPLE_INCLUDED

#include "global_uniforms.glsl"
#include "poisson.glsl"
#include "random.glsl"


float luminance_ups(vec3 c) {
	vec3 f = vec3(0.299,0.587,0.114);
	return sqrt(c.r*c.r*f.r + c.g*c.g*f.g + c.b*c.b*f.b);
}

float weight_offset(float x, float sharpness) {
	float b = 0;
	float c = (2.0*2.0) * 4 / sharpness;
	return 0.5 * exp(- (x-b)*(x-b) / (2*c*c));
}
float weight_depth(float x, float depth_dev) {
	float b = 0;
	float c = depth_dev;
	return 0.2 * exp(- (x-b)*(x-b) / (2*c*c));
}

vec4 upsampled_result(sampler2D depth_sampler, sampler2D color_sampler, int depth_lod, int color_lod, vec2 tex_coords, float sharpness) {
	vec2 color_size = textureSize(color_sampler, color_lod);
	vec2 depth_size = textureSize(depth_sampler, depth_lod);

	float depth = texelFetch(depth_sampler, ivec2(depth_size*tex_coords), depth_lod).r;
	float depth_dev = mix(0.05, 1.0, depth) / global_uniforms.proj_planes.y;
	
	float weight_sum = 0;
	vec4 color = vec4(0,0,0,0);
	
	for(float x=-2; x<=2; x++) {
		for(float y=-2; y<=2; y++) {
			vec2 offset = vec2(x,y);
			vec2 p = tex_coords + offset / color_size;
			
			float d = texelFetch(depth_sampler, ivec2(depth_size * p), depth_lod).r;
			
			float weight = weight_offset(dot(offset,offset), sharpness) * weight_depth(depth-d, depth_dev);
			
			vec4 c = texelFetch(color_sampler, ivec2(color_size * p), color_lod);
			color += weight * c / (1 + luminance_ups(c.rgb));
			weight_sum += weight;
		}
	}
	
	color /= weight_sum;
	return color / (1 - luminance_ups(color.rgb));
}

#endif
