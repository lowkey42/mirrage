#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable


layout(location = 0) in Vertex_data {
	vec2 tex_coords;
	vec3 view_ray;

	flat vec3 corner_view_rays[4];
} vertex_out;

layout(location = 0) out vec4 out_color;

layout(set=1, binding = 0) uniform sampler2D color_sampler;
layout(set=1, binding = 1) uniform sampler2D depth_sampler;
layout(set=1, binding = 2) uniform sampler2D mat_data_sampler;
layout(set=1, binding = 3) uniform sampler2D result_sampler;
layout(set=1, binding = 4) uniform sampler2D albedo_sampler;

layout (constant_id = 0) const bool LAST_SAMPLE = false;
layout (constant_id = 1) const float R = 40;
layout (constant_id = 2) const int SAMPLES = 128;

layout(push_constant) uniform Push_constants {
	mat4 reprojection;
	vec4 arguments;
} pcs;


#include "global_uniforms.glsl"
#include "normal_encoding.glsl"
#include "random.glsl"
#include "brdf.glsl"
#include "upsample.glsl"


vec3 gi_sample();
vec3 calc_illumination_from(vec2 src_uv, vec2 shaded_uv, float shaded_depth, vec3 shaded_point, vec3 shaded_normal,
                            out float weight);

void main() {
	out_color = vec4(upsampled_prev_result(pcs.arguments.x, vertex_out.tex_coords), 1.0);
	out_color.rgb += gi_sample();
}

const float PI = 3.14159265359;

vec3 gi_sample() {
	float lod = pcs.arguments.x;

	float depth  = textureLod(depth_sampler, vertex_out.tex_coords, lod).r;
	vec4 mat_data = textureLod(mat_data_sampler, vertex_out.tex_coords, lod);
	vec3 N = decode_normal(mat_data.rg);

	vec3 P = depth * vertex_out.view_ray;

	vec2 texture_size = textureSize(color_sampler, int(lod));

	vec3 c = vec3(0,0,0);
	float samples_used = 0.0;

	for(int i=0; i<SAMPLES; i++) {
		float r = mix(LAST_SAMPLE ? 0.0 : R/2.0, R, random(vec4(vertex_out.tex_coords, float(i), 0.0)));

		float angle = float(i) / float(SAMPLES) * PI * 2.0;
		float sin_angle = sin(angle);
		float cos_angle = cos(angle);

		vec2 p = vertex_out.tex_coords + vec2(sin(angle), cos(angle)) * r / texture_size;
		if(p.x>=0.0 && p.x<=1.0 && p.y>=0.0 && p.y<=1.0) {
			float weight;
			c += calc_illumination_from(p, vertex_out.tex_coords, depth, P, N, weight);
			samples_used += weight;
		}
	}

	// could be used to blend between screen-space and static GI
	//   float visibility = 1.0 - (samples_used / float(SAMPLES));

	return c * 2.0*PI / max(samples_used, SAMPLES*0.2);
}

vec3 to_view_space(vec2 uv, float depth) {
	vec3 view_ray_x1 = mix(vertex_out.corner_view_rays[0], vertex_out.corner_view_rays[1], uv.x);
	vec3 view_ray_x2 = mix(vertex_out.corner_view_rays[2], vertex_out.corner_view_rays[3], uv.x);

	return mix(view_ray_x1, view_ray_x2, uv.y) * depth;
}

vec3 calc_illumination_from(vec2 src_uv, vec2 shaded_uv, float shaded_depth, vec3 shaded_point, vec3 shaded_normal,
                            out float weight) {
	float lod = pcs.arguments.x;

	float depth  = textureLod(depth_sampler, src_uv, lod).r;
	vec4 mat_data = textureLod(mat_data_sampler, src_uv, lod);
	vec3 N = decode_normal(mat_data.rg);

	vec3 P = to_view_space(src_uv, depth);
	vec3 diff = shaded_point - P;
	float r = length(diff);
	if(r<0.0001) { // ignore too close pixels
		weight = 0.0;
		return vec3(0,0,0);
	}

	float r2 = r*r * 0.001;
	vec3 dir = diff / r;

	vec3 radiance = min(vec3(4,4,4), textureLod(color_sampler, src_uv, lod).rgb);

	float visibility = 1.0; // TODO: raycast

	float NdotL_src = clamp(dot(N, dir), 0.0, 1.0); // cos(θ')
	float NdotL_dst = clamp(dot(shaded_normal, -dir), 0.0, 1.0); // cos(θ)


	float normal_bias = clamp(1.0-dot(N,shaded_normal), 0.0, 1.0);
	float perspective_bias = 1.0 + clamp(1.0 - dot(-normalize(P), N), 0, 1)*0.5;

	visibility = visibility * NdotL_dst*NdotL_src * normal_bias * perspective_bias;

	weight = visibility > 0.0 ? 1.0 : 0.0;


	return max(vec3(0.0), radiance * visibility / max(1.0, r2));
}

