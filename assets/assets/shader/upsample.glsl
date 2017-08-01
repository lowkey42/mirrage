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
	return 0.3 * exp(- (x-b)*(x-b) / (2*c*c));
}
float weight_mat_data(float x) {
	return max(0.005, 1 - smoothstep(0.1, 0.3, x));
}

vec4 upsampled_result(sampler2D depth_sampler, sampler2D mat_data_sampler, sampler2D color_sampler,
                      int depth_lod, int color_lod, vec2 tex_coords, float sharpness) {
	vec2 color_size    = textureSize(color_sampler, color_lod);
	vec2 depth_size    = textureSize(depth_sampler, 0);
	vec2 mat_data_size = textureSize(mat_data_sampler, depth_lod);

	float depth = texelFetch(depth_sampler, ivec2(textureSize(depth_sampler, 0)*tex_coords), 0).r;
	float depth_dev = mix(0.05, 1.5, depth) / global_uniforms.proj_planes.y;

	vec2 normal = texelFetch(mat_data_sampler, ivec2(textureSize(mat_data_sampler, 0)*tex_coords), 0).xy;


	float weight_sum = 0;
	vec4 color = vec4(0,0,0,0);
	
	for(float x=-2; x<=2; x++) {
		for(float y=-2; y<=2; y++) {
			vec2 offset = vec2(x,y);
			vec2 p = tex_coords + offset / color_size;
			
			float d = texelFetch(depth_sampler, ivec2(depth_size * p), 0).r;
			vec2  n = texelFetch(mat_data_sampler, ivec2(mat_data_size * p), depth_lod).xy;
			vec2 normal_diff = normal - n;
			
			float weight = weight_offset(dot(offset,offset), sharpness)
			             * weight_depth(depth-d, depth_dev)
			             * weight_mat_data(dot(normal_diff, normal_diff));

			vec4 c = texelFetch(color_sampler, ivec2(color_size * p), color_lod);
			color += weight * c / (1 + luminance_ups(c.rgb));
			weight_sum += weight;
		}
	}
	
	if(weight_sum<0.01)
		return vec4(0,0,0,0);

	color /= weight_sum;
	return color / (1 - luminance_ups(color.rgb));
}

#endif
