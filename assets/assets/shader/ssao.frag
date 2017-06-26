#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable


layout(location = 0) in Vertex_data {
	vec2 tex_coords;
	vec3 view_ray;
	flat vec3 corner_view_rays[4];
} vertex_out;

layout(location = 0) out vec4 out_color;

layout (constant_id = 0) const int SAMPLES = 16;
layout (constant_id = 1) const int LOG_MAX_OFFSET = 4;
layout (constant_id = 2) const int SPIRAL_TURNS = 7;
layout (constant_id = 3) const float RADIUS = 1.1;
layout (constant_id = 4) const float BIAS = 0.01;

layout(set=1, binding = 0) uniform sampler2D depth_sampler;
layout(set=1, binding = 1) uniform sampler2D mat_data_sampler;

layout(push_constant) uniform Push_constants {
	vec4 options; // max_mip_level, proj_scale, min_mip_level
} pcs;

#define MIN_MIP int(pcs.options.z)


#include "global_uniforms.glsl"
#include "normal_encoding.glsl"
#include "poisson.glsl"
#include "random.glsl"

// heavily based on http://graphics.cs.williams.edu/papers/SAOHPG12/McGuire12SAO.pdf

vec3 to_view_space(ivec2 ss_p, int mip) {
	vec2 uv = vec2(ss_p) / textureSize(depth_sampler, MIN_MIP);

	ss_p = clamp(ss_p >> mip, ivec2(0), textureSize(depth_sampler, mip+MIN_MIP) - ivec2(1));
	float depth = texelFetch(depth_sampler, ss_p, mip+MIN_MIP).r;

	vec3 view_ray_x1 = mix(vertex_out.corner_view_rays[0], vertex_out.corner_view_rays[1], uv.x);
	vec3 view_ray_x2 = mix(vertex_out.corner_view_rays[2], vertex_out.corner_view_rays[3], uv.x);

	return mix(view_ray_x1, view_ray_x2, uv.y) * depth;
}

vec2 to_uv(vec3 pos) {
	vec4 p = global_uniforms.proj_mat * vec4(pos, 1.0);
	return (p.xy / p.w) * 0.5 + 0.5;
}

/** Returns a unit vector and a screen-space radius for the tap on a unit disk (the caller should scale by the actual disk radius) */
vec2 tap_location(int i, float spin_angle, out float out_ss_radius) {
	// Radius relative to ssR
	float alpha = float(i + 0.5) * (1.0 / SAMPLES);
	float angle = alpha * (SPIRAL_TURNS * 6.28) + spin_angle;

	out_ss_radius = alpha;
	return vec2(cos(angle), sin(angle));
}

/** Read the camera-space position of the point at screen-space pixel ssP + unitOffset * ssR.  Assumes length(unitOffset) == 1 */
vec3 get_offset_position(ivec2 ss_center, vec2 dir, float ss_radius) {
	int mip = clamp(int(floor(log2(ss_radius))) - LOG_MAX_OFFSET, 0, int(pcs.options.x));

	return to_view_space(ivec2(ss_center + dir*ss_radius), mip);
}

/** Compute the occlusion due to sample with index \a i about the pixel at \a ssC that corresponds
    to camera-space point \a C with unit normal \a n_C, using maximum screen-space sampling radius \a ssDiskRadius */
float sample_ao(ivec2 ss_center, vec3 C, vec3 n_C, float ss_disk_radius, int i, float random_pattern_rotation_angle) {
	// Offset on the unit disk, spun for this pixel
	float ss_r;
	vec2 unit_offset = tap_location(i, random_pattern_rotation_angle, ss_r);
	ss_r *= ss_disk_radius;

	// The occluding point in camera space
	vec3 Q = get_offset_position(ss_center, unit_offset, ss_r);

	vec3 v = Q - C;

	float vv = dot(v, v);
	float vn = dot(v, n_C);

	float border_fade_factor = clamp(abs(min(0.0, Q.z)), 0.0, 1.0);

	const float epsilon = 0.01;

//	if(vv>0.8)
	float c = 4.0 * max(1.0 - vv / (RADIUS*RADIUS), 0.0) * max(vn - BIAS, 0.0) * border_fade_factor;

	float f = max(RADIUS*RADIUS - vv, 0.0);

	float b = f * f * f * max((vn - BIAS) / (epsilon + vv), 0.0) * border_fade_factor;

	return mix(c,b, 0.5);
}

/** Used for packing Z into the GB channels */
float CSZToKey(float z) {
	return clamp(z * (1.0 / global_uniforms.proj_planes.y), 0.0, 1.0);
}

/** Used for packing Z into the GB channels */
void packKey(float key, out vec2 p) {
	// Round to the nearest 1/256.0
	float temp = floor(key * 256.0);

	// Integer part
	p.x = temp * (1.0 / 256.0);

	// Fractional part
	p.y = key * 256.0 - temp;
}


void main() {
	ivec2 center_px = ivec2(vertex_out.tex_coords*textureSize(depth_sampler, MIN_MIP));

	float depth = texelFetch(depth_sampler, center_px, MIN_MIP).r;
	vec3 P = depth * vertex_out.view_ray;

	packKey(CSZToKey(P.z), out_color.gb);

	// TODO/TEST: reconstruct face normals from depth
	vec3 N = decode_normal(texelFetch(mat_data_sampler, center_px, 0).rg);//*/normalize(cross(dFdy(P), dFdx(P)));

	// Hash function used in the HPG12 AlchemyAO paper
	float random_pattern_rotation_angle = (3 * center_px.x ^ center_px.y + center_px.x * center_px.y) * 10;

	// Choose the screen-space sample radius
	// proportional to the projected area of the sphere
	float ss_disk_radius = -pcs.options.y * RADIUS / P.z;

	float sum = 0.0;
	for (int i = 0; i < SAMPLES; ++i) {
		 sum += sample_ao(center_px, P, N, ss_disk_radius, i, random_pattern_rotation_angle);
	}

	float temp = RADIUS * RADIUS * RADIUS;
	sum /= temp * temp;
	out_color.r = max(0.0, 1.0 - sum * (5.0 / SAMPLES));
	out_color.r*=out_color.r * out_color.r;


/*
	// Bilateral box-filter over a quad for free, respecting depth edges
	// (the difference that this makes is subtle)
	if (abs(dFdx(P.z)) < 0.02) {
		out_color.r -= dFdx(out_color.r) * ((center_px.x & 1) - 0.5);
	}
	if (abs(dFdy(P.z)) < 0.02) {
		out_color.r -= dFdy(out_color.r) * ((center_px.y & 1) - 0.5);
	}
*/

/*
	vec3 random = normalize(vec3(PDnrand3(vertex_out.tex_coords).xy*2-1, 0.0));
	vec3 tangent = normalize(random - N * dot(random, N));
	vec3 bitangent = cross(N, tangent);
	mat3 TBN = mat3(tangent, bitangent, N);

	// iterate over the sample kernel and calculate occlusion factor
	float occlusion = 0.0;
	for(int i = 0; i < KERNEL_SIZE; ++i) {
		// expected view space position of sample
		vec3 expected = P + TBN * samples[i] * RADIUS;

		// actual view space position of sample (reconstructed from gBuffer depth)
		vec3 actual = to_view_space(to_uv(expected));

		// range check & accumulate
		float range_check = 1.0 - smoothstep(0.0, 1.0, RADIUS / abs(P.z - actual.z));
		range_check = 1.0 - range_check*range_check;
		range_check *= clamp(abs(min(0.0, actual.z)), 0.0, 1.0);
		occlusion += (actual.z > expected.z + BIAS ? 1.0 : 0.0) * range_check;
	}
	out_color.r = 1.0 - (occlusion / KERNEL_SIZE);
	out_color.r = out_color.r * out_color.r;
	*/
}
