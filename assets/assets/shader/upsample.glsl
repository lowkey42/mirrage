
#include "poisson.glsl"


float linearize_depth(float depth) {
	float near_plane = global_uniforms.proj_planes.x;
	float far_plane = global_uniforms.proj_planes.y;
	return (2.0 * near_plane)
	     / (far_plane + near_plane - depth * (far_plane - near_plane));
}

vec3 upsampled_prev_result(float current_lod, vec2 tex_coords) { // TODO: still too noisy
	float lod = current_lod + 0.9999999;
	if(lod>4.0)
		return vec3(0,0,0);

	vec2 texture_size = textureSize(result_sampler, int(lod));

	float depth = linearize_depth(textureLod(depth_sampler, tex_coords, lod-1).r);

	const vec2 offset[4] = vec2[](
		vec2(+1, +1),
		vec2(+1, -1),
		vec2(-1, -1),
		vec2(-1, +1)
	);

	vec3 c = vec3(0,0,0);
	float weight_sum = 0.0;
	for(int i=0; i<8; i++) {
		vec2 uv = tex_coords + Poisson8[i]/texture_size*2.0;
		float d = linearize_depth(textureLod(depth_sampler, uv, lod-1).r);

		float weight = 1.0 / (0.0001 + abs(depth-d));

		c += weight * textureLod(result_sampler, uv, lod).rgb;
		weight_sum += weight;
	}

	return c / weight_sum;
}
