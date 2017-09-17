#ifndef UPSAMPLE_INCLUDED
#define UPSAMPLE_INCLUDED

#include "global_uniforms.glsl"
#include "poisson.glsl"
#include "random.glsl"

vec4 weight_depth(vec4 x, float depth_dev) {
	float c = depth_dev;
	return exp(- x*x / (2*c*c));
}
vec4 weight_mat_data(vec4 dx, vec4 dy) {
	return max(vec4(0.005), 1 - smoothstep(0.1, 0.3, dx*dx+dy*dy));
}

float calc_upsampled_weights(sampler2D highres_depth_sampler, sampler2D highres_mat_data_sampler,
                             sampler2D depth_sampler, sampler2D mat_data_sampler, vec2 tex_coords,
                             out vec2 uv_00, out vec2 uv_10, out vec2 uv_11, out vec2 uv_01,
                             out vec4 weight_00, out vec4 weight_10, out vec4 weight_11, out vec4 weight_01) {

    vec2 tex_size = textureSize(depth_sampler, 0);

	float depth = texelFetch(highres_depth_sampler, ivec2(textureSize(highres_depth_sampler, 0)*tex_coords), 0).r;
	float depth_dev = mix(0.5, 2.0, depth) / global_uniforms.proj_planes.y;

	vec2 normal = texelFetch(highres_mat_data_sampler, ivec2(textureSize(highres_mat_data_sampler, 0)*tex_coords), 0).xy;


	uv_00 = tex_coords + vec2(-1,-1) / tex_size;
	uv_10 = tex_coords + vec2( 1,-1) / tex_size;
	uv_11 = tex_coords + vec2( 1, 1) / tex_size;
	uv_01 = tex_coords + vec2(-1, 1) / tex_size;

	weight_00 = vec4(0.125794409230998,
	                 0.132980760133811,
	                 0.125794409230998,
	                 0.118996412547595);
	weight_10 = vec4(0.125794409230998,
	                 0.106482668507451,
	                 0.100728288549083,
	                 0.118996412547595);
	weight_11 = vec4(0.100728288549083,
	                 0.085264655436308,
	                 0.100728288549083,
	                 0.118996412547595);
	weight_01 = vec4(0.100728288549083,
	                 0.106482668507451,
	                 0.125794409230998,
	                 0.118996412547595);

	weight_00 *= weight_depth(textureGather(depth_sampler, uv_00, 0) - depth, depth_dev);
	weight_10 *= weight_depth(textureGather(depth_sampler, uv_10, 0) - depth, depth_dev);
	weight_11 *= weight_depth(textureGather(depth_sampler, uv_11, 0) - depth, depth_dev);
	weight_01 *= weight_depth(textureGather(depth_sampler, uv_01, 0) - depth, depth_dev);

	vec4 normal_x_00 = textureGather(mat_data_sampler, uv_00, 0) - normal.x;
	vec4 normal_x_10 = textureGather(mat_data_sampler, uv_10, 0) - normal.x;
	vec4 normal_x_11 = textureGather(mat_data_sampler, uv_11, 0) - normal.x;
	vec4 normal_x_01 = textureGather(mat_data_sampler, uv_01, 0) - normal.x;

	vec4 normal_y_00 = textureGather(mat_data_sampler, uv_00, 1) - normal.y;
	vec4 normal_y_10 = textureGather(mat_data_sampler, uv_10, 1) - normal.y;
	vec4 normal_y_11 = textureGather(mat_data_sampler, uv_11, 1) - normal.y;
	vec4 normal_y_01 = textureGather(mat_data_sampler, uv_01, 1) - normal.y;

	weight_00 *= weight_mat_data(normal_x_00, normal_y_00);
	weight_10 *= weight_mat_data(normal_x_10, normal_y_10);
	weight_11 *= weight_mat_data(normal_x_11, normal_y_11);
	weight_01 *= weight_mat_data(normal_x_01, normal_y_01);


	return dot(weight_00, vec4(1))
	     + dot(weight_10, vec4(1))
	     + dot(weight_11, vec4(1))
	     + dot(weight_01, vec4(1));
}

vec3 upsampled_result(sampler2D highres_depth_sampler, sampler2D highres_mat_data_sampler,
                      sampler2D depth_sampler, sampler2D mat_data_sampler,
                      sampler2D color_sampler, vec2 tex_coords) {
	vec2 uv_00;
	vec2 uv_10;
	vec2 uv_11;
	vec2 uv_01;

	vec4 weight_00;
	vec4 weight_10;
	vec4 weight_11;
	vec4 weight_01;

	float weight_sum = calc_upsampled_weights(highres_depth_sampler, highres_mat_data_sampler,
	                                          depth_sampler, mat_data_sampler, tex_coords,
	                                          uv_00, uv_10, uv_11, uv_01,
	                                          weight_00, weight_10, weight_11, weight_01);

	if(weight_sum<0.001)
		return textureLod(color_sampler, tex_coords, 0).rgb;


	float color_r = dot(vec4(1),
	                textureGather(color_sampler, uv_00, 0) * weight_00
	              + textureGather(color_sampler, uv_10, 0) * weight_10
	              + textureGather(color_sampler, uv_11, 0) * weight_11
	              + textureGather(color_sampler, uv_01, 0) * weight_01);

	float color_g = dot(vec4(1),
	                textureGather(color_sampler, uv_00, 1) * weight_00
	              + textureGather(color_sampler, uv_10, 1) * weight_10
	              + textureGather(color_sampler, uv_11, 1) * weight_11
	              + textureGather(color_sampler, uv_01, 1) * weight_01);

	float color_b = dot(vec4(1),
	                textureGather(color_sampler, uv_00, 2) * weight_00
	              + textureGather(color_sampler, uv_10, 2) * weight_10
	              + textureGather(color_sampler, uv_11, 2) * weight_11
	              + textureGather(color_sampler, uv_01, 2) * weight_01);

	vec3 color = vec3(color_r, color_g, color_b) / weight_sum;
	return color;
}


void upsampled_two(sampler2D highres_depth_sampler, sampler2D highres_mat_data_sampler,
                   sampler2D depth_sampler, sampler2D mat_data_sampler,
                   sampler2D color_sampler_a, sampler2D color_sampler_b, vec2 tex_coords,
                   out vec3 out_color_a, out vec3 out_color_b) {
	vec2 uv_00;
	vec2 uv_10;
	vec2 uv_11;
	vec2 uv_01;

	vec4 weight_00;
	vec4 weight_10;
	vec4 weight_11;
	vec4 weight_01;

	float weight_sum = calc_upsampled_weights(highres_depth_sampler, highres_mat_data_sampler,
	                                          depth_sampler, mat_data_sampler, tex_coords,
	                                          uv_00, uv_10, uv_11, uv_01,
	                                          weight_00, weight_10, weight_11, weight_01);

	if(weight_sum<0.001) {
		out_color_a = textureLod(color_sampler_a, tex_coords, 0).rgb;
		out_color_b = textureLod(color_sampler_b, tex_coords, 0).rgb;
	}


	float color_r = dot(vec4(1),
	                textureGather(color_sampler_a, uv_00, 0) * weight_00
	              + textureGather(color_sampler_a, uv_10, 0) * weight_10
	              + textureGather(color_sampler_a, uv_11, 0) * weight_11
	              + textureGather(color_sampler_a, uv_01, 0) * weight_01);

	float color_g = dot(vec4(1),
	                textureGather(color_sampler_a, uv_00, 1) * weight_00
	              + textureGather(color_sampler_a, uv_10, 1) * weight_10
	              + textureGather(color_sampler_a, uv_11, 1) * weight_11
	              + textureGather(color_sampler_a, uv_01, 1) * weight_01);

	float color_b = dot(vec4(1),
	                textureGather(color_sampler_a, uv_00, 2) * weight_00
	              + textureGather(color_sampler_a, uv_10, 2) * weight_10
	              + textureGather(color_sampler_a, uv_11, 2) * weight_11
	              + textureGather(color_sampler_a, uv_01, 2) * weight_01);

	out_color_a = vec3(color_r, color_g, color_b) / weight_sum;


	color_r = dot(vec4(1),
	          textureGather(color_sampler_b, uv_00, 0) * weight_00
	        + textureGather(color_sampler_b, uv_10, 0) * weight_10
	        + textureGather(color_sampler_b, uv_11, 0) * weight_11
	        + textureGather(color_sampler_b, uv_01, 0) * weight_01);

	color_g = dot(vec4(1),
	          textureGather(color_sampler_b, uv_00, 1) * weight_00
	        + textureGather(color_sampler_b, uv_10, 1) * weight_10
	        + textureGather(color_sampler_b, uv_11, 1) * weight_11
	        + textureGather(color_sampler_b, uv_01, 1) * weight_01);

	color_b = dot(vec4(1),
	          textureGather(color_sampler_b, uv_00, 2) * weight_00
	        + textureGather(color_sampler_b, uv_10, 2) * weight_10
	        + textureGather(color_sampler_b, uv_11, 2) * weight_11
	        + textureGather(color_sampler_b, uv_01, 2) * weight_01);

	out_color_b = vec3(color_r, color_g, color_b) / weight_sum;
}

#endif
