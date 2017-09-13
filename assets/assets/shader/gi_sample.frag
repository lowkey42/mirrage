#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable


layout(location = 0) in Vertex_data {
	vec2 tex_coords;
} vertex_out;

layout(location = 0) out vec4 out_color;

layout(set=1, binding = 0) uniform sampler2D color_sampler;
layout(set=1, binding = 1) uniform sampler2D depth_sampler;
layout(set=1, binding = 2) uniform sampler2D mat_data_sampler;
layout(set=1, binding = 3) uniform sampler2D result_sampler;
layout(set=1, binding = 4) uniform sampler2D history_weight_sampler;
layout(set=1, binding = 5) uniform sampler2D prev_depth_sampler;
layout(set=1, binding = 6) uniform sampler2D prev_mat_data_sampler;

layout (constant_id = 0) const int LAST_SAMPLE = 0;
layout (constant_id = 1) const float R = 40;
layout (constant_id = 2) const int SAMPLES = 128;
layout (constant_id = 3) const int UPSAMPLE_ONLY = 0;

// nearer samples have a higher weight. Less physically correct but results in more notacable color bleeding
layout (constant_id = 4) const int PRIORITISE_NEAR_SAMPLES = 1;


layout(push_constant) uniform Push_constants {
	mat4 projection;
	mat4 prev_projection;
} pcs;


#include "global_uniforms.glsl"
#include "normal_encoding.glsl"
#include "random.glsl"
#include "brdf.glsl"
#include "upsample.glsl"
#include "raycast.glsl"

vec3 gi_sample(int lod, int base_mip);
vec3 calc_illumination_from(int lod, vec2 tex_size, ivec2 src_uv, vec2 shaded_uv, float shaded_depth,
                            vec3 shaded_point, vec3 shaded_normal, out float weight);

void main() {
	float current_mip = pcs.prev_projection[0][3];
	float max_mip     = pcs.prev_projection[1][3];
	float base_mip    = pcs.prev_projection[3][3];

	if(current_mip < max_mip)
		out_color = vec4(upsampled_result(depth_sampler, mat_data_sampler,
		                                  prev_depth_sampler, prev_mat_data_sampler,
					                      result_sampler, vertex_out.tex_coords), 1.0);
	else
		out_color = vec4(0,0,0, 1);

	if(UPSAMPLE_ONLY==0)
		out_color.rgb += gi_sample(int(current_mip+0.5), int(base_mip+0.5));

	// last mip level => blend with history
	if(abs(current_mip - base_mip) < 0.00001) {
		float history_weight = texelFetch(history_weight_sampler,
		                                  ivec2(vertex_out.tex_coords * textureSize(history_weight_sampler, 0)),
		                                  0).r;

		out_color *= 1.0 - (history_weight*0.85);
	}

	out_color = max(out_color, vec4(0));
}

const float PI = 3.14159265359;
const float REC_PI = 0.3183098861837906715;

vec3 saturation(vec3 c, float change) {
	vec3 f = vec3(0.299,0.587,0.114);
	float p = sqrt(c.r*c.r*f.r + c.g*c.g*f.g + c.b*c.b*f.b);

	return vec3(p) + (c-vec3(p))*vec3(change);
}


vec3 gi_sample(int lod, int base_mip) {
	vec2 texture_size = textureSize(color_sampler, 0);
	ivec2 uv = ivec2(vertex_out.tex_coords * texture_size);

	// workaround for white bar at right/bottom border
	if(uv.y >= texture_size.y-1 || uv.x >= texture_size.x-1)
		return vec3(0, 0, 0);

	float depth  = texelFetch(depth_sampler, uv, 0).r;
	vec4 mat_data = texelFetch(mat_data_sampler, uv, 0);
	vec3 N = decode_normal(mat_data.rg);

	vec3 P = position_from_ldepth(vertex_out.tex_coords, depth);

	vec3 c = vec3(0,0,0);
	float samples_used = 0.0;
	float angle = random(vec4(vertex_out.tex_coords, 0.0, 0));
	float angle_step = 1.0 / float(SAMPLES) * PI * 2.0 * 19.0;

	for(int i=0; i<SAMPLES; i++) {
		float r = mix(LAST_SAMPLE==1 ? 2.0 : R/2.0, R, float(i)/float(SAMPLES));

		angle += angle_step;
		float sin_angle = sin(angle);
		float cos_angle = cos(angle);

		ivec2 p = ivec2(uv + vec2(sin_angle, cos_angle) * r);
//		if(p.x>0.0 && p.x<texture_size.x && p.y>0.0 && p.y<texture_size.y) {
			float weight;
			vec3 sc = calc_illumination_from(lod, texture_size, p, uv, depth, P, N, weight);

			c += sc;
			samples_used += weight;
//		}
	}

	// could be used to blend between screen-space and static GI
	//   float visibility = 1.0 - (samples_used / float(SAMPLES));

	if(PRIORITISE_NEAR_SAMPLES==1)
		c = c * pow(2.0, clamp((lod-base_mip)*2, 4, 7));
	else
		c = c * pow(2.0, (lod-base_mip)*2);

	c  *= 128.0 / SAMPLES;

	return c;
}

vec3 calc_illumination_from(int lod, vec2 tex_size, ivec2 src_uv, vec2 shaded_uv, float shaded_depth,
                            vec3 shaded_point, vec3 shaded_normal, out float weight) {
	float depth  = texelFetch(depth_sampler, src_uv, 0).r;
	vec3 P = position_from_ldepth(src_uv / tex_size, depth); // x_i
	vec3 Pn = normalize(P);

	vec3 diff = shaded_point - P;
	vec3 dir = normalize(diff);
	float r2 = dot(diff, diff) * 1.2;

	float visibility = 1.0; // TODO: raycast

	vec4 mat_data = texelFetch(mat_data_sampler, src_uv, 0);
	vec3 N = decode_normal(mat_data.rg);

	vec3 radiance = texelFetch(color_sampler, src_uv, 0).rgb;

	float NdotL_src = clamp(dot(N, dir), 0.0, 1.0); // cos(θ')
	float NdotL_dst = clamp(dot(shaded_normal, -dir), 0.0, 1.0); // cos(θ)

	float cos_alpha = Pn.z;
	float cos_beta  = dot(Pn, N);
	float z = depth * global_uniforms.proj_planes.y;

	float ds = pcs.prev_projection[2][3] * z*z * clamp(cos_alpha / cos_beta, 1.0, 20.0);

	float R2 = REC_PI * NdotL_src * ds;
	float area = R2 / (r2 + R2); // point-to-differential area form-factor

	weight = visibility * NdotL_dst * area;

	return max(vec3(0.0), radiance * weight);
}

