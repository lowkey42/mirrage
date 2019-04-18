#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

// Based on http://graphics.cs.williams.edu/papers/SAOHPG12/McGuire12SAO.pdf

layout(location = 0) in Vertex_data {
	vec2 tex_coords;
} vertex_out;

layout(location = 0) out vec4 out_color;

layout (constant_id = 0) const int SAMPLES = 16;
layout (constant_id = 1) const int LOG_MAX_OFFSET = 4;
layout (constant_id = 2) const int SPIRAL_TURNS = 7;
layout (constant_id = 3) const float RADIUS = 1.0;
layout (constant_id = 4) const float BIAS = 0.03;

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

vec3 to_view_space(ivec2 ss_p, int mip) {
	vec2 uv = vec2(ss_p) / textureSize(depth_sampler, MIN_MIP);

	ss_p = clamp(ss_p >> mip, ivec2(0), textureSize(depth_sampler, mip+MIN_MIP) - ivec2(1));
	float depth = texelFetch(depth_sampler, ss_p, mip+MIN_MIP).r;

	return position_from_ldepth(uv, depth);
}

vec3 get_normal(ivec2 ss_p, int mip) {
	return decode_normal(texelFetch(mat_data_sampler, ss_p, 0).rg);

	ss_p = clamp(ss_p >> mip, ivec2(0), textureSize(mat_data_sampler, mip+MIN_MIP) - ivec2(1));
	return decode_normal(texelFetch(mat_data_sampler, ss_p, mip+MIN_MIP).rg);
}

vec2 to_uv(vec3 pos) {
	vec4 p = global_uniforms.proj_mat * vec4(pos, 1.0);
	return (p.xy / p.w) * 0.5 + 0.5;
}

/** Returns a unit vector and a screen-space radius for the tap on a unit disk (the caller should scale by the actual disk radius) */
vec2 tap_location(int i, float spin_angle, out float out_ss_radius) {
	// Radius relative to ssR
	float alpha = float(i + 0.5) * (1.0 / SAMPLES);
	float angle = i * (3.14159265359*(3.0-sqrt(5.0))) + spin_angle;

	out_ss_radius = alpha;
	return vec2(cos(angle), sin(angle));
}

/** Compute the occlusion due to sample with index \a i about the pixel at \a ssC that corresponds
    to camera-space point \a C with unit normal \a n_C, using maximum screen-space sampling radius \a ssDiskRadius */
float sample_ao(ivec2 ss_center, vec3 C, vec3 n_C, float ss_disk_radius, int i, float random_pattern_rotation_angle) {
	// Offset on the unit disk, spun for this pixel
	float ss_r;
	vec2 unit_offset = tap_location(i, random_pattern_rotation_angle, ss_r);
	ss_r *= ss_disk_radius;

	// The occluding point in camera space
	int mip = clamp(int(floor(log2(ss_disk_radius))) - LOG_MAX_OFFSET, 0, int(pcs.options.x));
	ivec2 ss_p = ivec2(ss_center + unit_offset*ss_r);

	vec3 Q = to_view_space(ss_p, mip);

	vec3 v = Q - C;

	float vv = max(0, dot(v, v));
	float vn = max(0, dot(v, n_C));

	const float epsilon = 0.01;

	vec3 N = get_normal(ss_p, mip);
	float occluder_angle = abs(dot(N, n_C));
	float boost = smoothstep(0.01, 0.2, occluder_angle)*0.6+0.4;
	boost += smoothstep(0.8, 1.0, occluder_angle);

	float f = max(RADIUS*RADIUS - vv, 0.0);

	return f * f * f * max((vn - BIAS) / (epsilon + vv), 0.0) * boost;
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
	ivec2 center_px_normal = ivec2(vertex_out.tex_coords*textureSize(mat_data_sampler, 0));

	float depth = texelFetch(depth_sampler, center_px, MIN_MIP).r;
	vec3 P = position_from_ldepth(vertex_out.tex_coords, depth);
	vec3 N = decode_normal(texelFetch(mat_data_sampler, center_px_normal, 0).rg);
	P += N*0.05;

	packKey(CSZToKey(P.z), out_color.gb);

	float random_pattern_rotation_angle = random(vec4(center_px.x, center_px.y, global_uniforms.time.x, 0));

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

	// Bilateral box-filter over a quad for free, respecting depth edges
	// (the difference that this makes is subtle)
	if (abs(dFdx(P.z)) < 0.05) {
		out_color.r -= dFdx(out_color.r) * ((center_px.x & 1) - 0.5);
	}
	if (abs(dFdy(P.z)) < 0.05) {
		out_color.r -= dFdy(out_color.r) * ((center_px.y & 1) - 0.5);
	}
}
