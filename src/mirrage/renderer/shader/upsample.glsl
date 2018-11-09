#ifndef UPSAMPLE_INCLUDED
#define UPSAMPLE_INCLUDED

#include "global_uniforms.glsl"
#include "normal_encoding.glsl"

// calculate a weighting factor based on the normal differences x and the deviation
vec4 weight_depth(vec4 x, float dev) {
	return exp(-x*x / (2*dev*dev));
}

// calculate a weighting factor based on the difference of the encoded normals
vec4 weight_mat_data(vec4 dx, vec4 dy) {
	return max(vec4(0.005), 1 - smoothstep(0.05, 0.2, (dx*dx+dy*dy)));
}
vec4 weight_normal(vec3 normal, vec4 dx, vec4 dy) {
	return vec4(smoothstep(0.2, 0.8, dot(normal, decode_normal(vec2(dx[0], dy[0])))),
	        smoothstep(0.2, 0.8, dot(normal, decode_normal(vec2(dx[1], dy[1])))),
	        smoothstep(0.2, 0.8, dot(normal, decode_normal(vec2(dx[2], dy[2])))),
	        smoothstep(0.2, 0.8, dot(normal, decode_normal(vec2(dx[3], dy[3])))));

	//return max(vec4(0.005), 1 - smoothstep(0.05, 0.2, (dx*dx+dy*dy)));
}

// calculate the uv coordinates of the 2x2 blocks to sample and the per-pixel weights based on normal/depth
// returns the sum of all per-pixel weights
float calc_upsampled_weights(sampler2D highres_depth_sampler, sampler2D highres_mat_data_sampler,
                             sampler2D depth_sampler,         sampler2D mat_data_sampler,
                             vec2 tex_coords,
                             out vec2 uv_00,     out vec2 uv_10,     out vec2 uv_11,     out vec2 uv_01,
                             out vec4 weight_00, out vec4 weight_10, out vec4 weight_11, out vec4 weight_01) {
	// sample high-res depth + normal
	float depth = texelFetch(highres_depth_sampler,    ivec2(textureSize(highres_depth_sampler,    0)*tex_coords), 0).r;
	vec3 normal = decode_normal(texelFetch(highres_mat_data_sampler, ivec2(textureSize(highres_mat_data_sampler, 0)*tex_coords), 0).xy);

    // calculate uv coordinates
    vec2 tex_size = textureSize(depth_sampler, 0);
	uv_00 = tex_coords + vec2(-1,-1) / tex_size;
	uv_10 = tex_coords + vec2( 1,-1) / tex_size;
	uv_11 = tex_coords + vec2( 1, 1) / tex_size;
	uv_01 = tex_coords + vec2(-1, 1) / tex_size;

	// initialize the per-pixel weights with gaussian weights
	weight_00 = vec4(1,2,1,1)/20.0;
	weight_10 = vec4(2,1,1,1)/20.0;
	weight_11 = vec4(1,1,1,2)/20.0;
	weight_01 = vec4(1,1,2,1)/20.0;

	// calculate the maximum depth deviation based on the distance, to reduce bluring
	//   near the camera where it's most noticable
	float depth_dev = mix(0.1, 0.6, depth) / global_uniforms.proj_planes.y;

	// sample low-res depth and modulate the weights based on their difference to the high-res depth
	weight_00 *= weight_depth(textureGather(depth_sampler, uv_00, 0) - depth, depth_dev);
	weight_10 *= weight_depth(textureGather(depth_sampler, uv_10, 0) - depth, depth_dev);
	weight_11 *= weight_depth(textureGather(depth_sampler, uv_11, 0) - depth, depth_dev);
	weight_01 *= weight_depth(textureGather(depth_sampler, uv_01, 0) - depth, depth_dev);

	// sample the encoded low-res normals
	vec4 normal_x_00 = textureGather(mat_data_sampler, uv_00, 0);
	vec4 normal_x_10 = textureGather(mat_data_sampler, uv_10, 0);
	vec4 normal_x_11 = textureGather(mat_data_sampler, uv_11, 0);
	vec4 normal_x_01 = textureGather(mat_data_sampler, uv_01, 0);

	vec4 normal_y_00 = textureGather(mat_data_sampler, uv_00, 1);
	vec4 normal_y_10 = textureGather(mat_data_sampler, uv_10, 1);
	vec4 normal_y_11 = textureGather(mat_data_sampler, uv_11, 1);
	vec4 normal_y_01 = textureGather(mat_data_sampler, uv_01, 1);

	// modulate the weights based on normal difference
	weight_00 *= weight_normal(normal, normal_x_00, normal_y_00);
	weight_10 *= weight_normal(normal, normal_x_10, normal_y_10);
	weight_11 *= weight_normal(normal, normal_x_11, normal_y_11);
	weight_01 *= weight_normal(normal, normal_x_01, normal_y_01);

	// sum all per-pixel weights
	return dot(weight_00, vec4(1))
	     + dot(weight_10, vec4(1))
	     + dot(weight_11, vec4(1))
	     + dot(weight_01, vec4(1));
}

// calculate the uv coordinates of the 2x2 blocks to sample and the per-pixel weights based on normal/depth
// returns the sum of all per-pixel weights
float calc_small_upsampled_weights(sampler2D highres_depth_sampler, sampler2D highres_mat_data_sampler,
                                   sampler2D depth_sampler,         sampler2D mat_data_sampler,
                                   vec2 tex_coords,
                                   out vec2 uv, out vec4 weights) {
	// sample high-res depth + normal
	float depth = texelFetch(highres_depth_sampler,    ivec2(textureSize(highres_depth_sampler,    0)*tex_coords), 0).r;
	vec2 normal = texelFetch(highres_mat_data_sampler, ivec2(textureSize(highres_mat_data_sampler, 0)*tex_coords), 0).xy;

    // calculate uv coordinates
	uv = tex_coords;

	// initialize the per-pixel weights with gaussian weights
	weights = vec4(0.25);

	// calculate the maximum depth deviation based on the distance, to reduce bluring
	//   near the camera where it's most noticable
	float depth_dev = mix(0.1, 1.5, depth) / global_uniforms.proj_planes.y;

	// sample low-res depth and modulate the weights based on their difference to the high-res depth
	weights *= weight_depth(textureGather(depth_sampler, uv, 0) - depth, depth_dev);

	// sample the encoded low-res normals
	// modulate the weights based on normal difference
	weights *= weight_mat_data(textureGather(mat_data_sampler, uv, 0) - normal.x, textureGather(mat_data_sampler, uv, 1) - normal.y);

	// sum all per-pixel weights
	return dot(weights, vec4(1));
}

// calculate the high-res approximation of the given low-res solution (color_sampler) at a single point
//   using Join-Bilateral-Upsampling based on the high- and low-res normal and depth values
vec3 upsampled_result(sampler2D highres_depth_sampler, sampler2D highres_mat_data_sampler,
                      sampler2D depth_sampler,         sampler2D mat_data_sampler,
                      sampler2D color_sampler,         vec2 tex_coords) {
    // calculate the uv coordinates and per-pixel weights
	vec2  uv_00,     uv_10,     uv_11,     uv_01;
	vec4  weight_00, weight_10, weight_11, weight_01;
	float weight_sum = calc_upsampled_weights(highres_depth_sampler, highres_mat_data_sampler,
	                                          depth_sampler,         mat_data_sampler, tex_coords,
	                                          uv_00,     uv_10,      uv_11,     uv_01,
	                                          weight_00, weight_10,  weight_11, weight_01);

	// fallback to linear interpolation if no good match could be found in the low-res solution
	if(weight_sum<0.0001)
		return textureLod(color_sampler, tex_coords, 0).rgb*0.4;

	// gather the RGB values of the 16 surrounding pixels and weight them by the calcuated weights
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

	return vec3(color_r, color_g, color_b) / weight_sum;
}

// same as upsampled_result but upsamples to solutions at the same time
void upsampled_two(sampler2D highres_depth_sampler, sampler2D highres_mat_data_sampler,
                   sampler2D depth_sampler,         sampler2D mat_data_sampler,
                   sampler2D color_sampler_a,       sampler2D color_sampler_b, vec2 tex_coords,
                   out vec3  out_color_a,           out vec3  out_color_b) {
    // calculate the uv coordinates and per-pixel weights
	vec2  uv_00,     uv_10,     uv_11,     uv_01;
	vec4  weight_00, weight_10, weight_11, weight_01;
	float weight_sum = calc_upsampled_weights(highres_depth_sampler, highres_mat_data_sampler,
	                                          depth_sampler,         mat_data_sampler, tex_coords,
	                                          uv_00,     uv_10,      uv_11,     uv_01,
	                                          weight_00, weight_10,  weight_11, weight_01);

	// fallback to linear interpolation if no good match could be found in the low-res solution
	if(weight_sum<0.0001) {
		out_color_a = textureLod(color_sampler_a, tex_coords, 0).rgb*0.4;
		out_color_b = textureLod(color_sampler_b, tex_coords, 0).rgb*0.4;
		return;
	}

	// gather the RGB values of the 16 surrounding pixels and weight them by the calcuated weights
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

	// gather the RGB values of the 16 surrounding pixels and weight them by the calcuated weights
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

// faster upsampling using just 2x2 texels
void fast_upsampled_two(sampler2D highres_depth_sampler, sampler2D highres_mat_data_sampler,
                        sampler2D depth_sampler,         sampler2D mat_data_sampler,
                        sampler2D color_sampler_a,       sampler2D color_sampler_b, vec2 tex_coords,
                        out vec3  out_color_a,           out vec3  out_color_b) {
    // calculate the uv coordinates and per-pixel weights
	vec2  uv;
	vec4  weight;
	float weight_sum = calc_small_upsampled_weights(highres_depth_sampler, highres_mat_data_sampler,
	                                                depth_sampler,         mat_data_sampler, tex_coords,
	                                                uv, weight);

	// fallback to linear interpolation if no good match could be found in the low-res solution
	if(weight_sum<0.0001) {
		out_color_a = textureLod(color_sampler_a, tex_coords, 0).rgb*0.4;
		out_color_b = textureLod(color_sampler_b, tex_coords, 0).rgb*0.4;
		return;
	}

	// gather the RGB values of the 16 surrounding pixels and weight them by the calcuated weights
	float color_r = dot(vec4(1), textureGather(color_sampler_a, uv, 0) * weight);
	float color_g = dot(vec4(1), textureGather(color_sampler_a, uv, 1) * weight);
	float color_b = dot(vec4(1), textureGather(color_sampler_a, uv, 2) * weight);
	out_color_a = vec3(color_r, color_g, color_b) / weight_sum;

	// gather the RGB values of the 16 surrounding pixels and weight them by the calcuated weights
	color_r = dot(vec4(1), textureGather(color_sampler_b, uv, 0) * weight);
	color_g = dot(vec4(1), textureGather(color_sampler_b, uv, 1) * weight);
	color_b = dot(vec4(1), textureGather(color_sampler_b, uv, 2) * weight);
	out_color_b = vec3(color_r, color_g, color_b) / weight_sum;

}
#endif
