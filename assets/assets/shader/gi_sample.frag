#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable


layout(location = 0) in Vertex_data {
	vec2 tex_coords;
	vec3 view_ray;
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
vec3 importance_sample();
vec3 calc_illumination_from(vec2 src_point, vec3 shaded_point, vec3 shaded_normal,
                            vec3 shaded_albedo, vec3 shaded_F0, float shaded_roughness,
                            vec3 V, out float weight);

void main() {
	out_color = vec4(upsampled_prev_result(pcs.arguments.x, vertex_out.tex_coords), 1.0);

	if(LAST_SAMPLE) {
		out_color.rgb += gi_sample();
		//out_color.rgb += importance_sample();
		//out_color.rgb = textureLod(result_sampler, vertex_out.tex_coords, 2.0).rgb;

	} else {
		out_color.rgb += gi_sample();
	}
}

const float PI = 3.14159265359;

vec3 gi_sample() {
	float lod = pcs.arguments.x;

	vec3 albedo = textureLod(albedo_sampler, vertex_out.tex_coords, 0.0).rgb;
	float depth  = textureLod(depth_sampler, vertex_out.tex_coords, lod).r;
	vec4 mat_data = textureLod(mat_data_sampler, vertex_out.tex_coords, lod);
	vec3 N = decode_normal(mat_data.rg);
	float roughness = mat_data.b;
	float metallic = mat_data.a;

	vec3 F0 = mix(vec3(0.04), albedo.rgb, metallic);
	albedo.rgb *= 1.0 - metallic;

	vec3 P = depth * vertex_out.view_ray;
	vec3 V = normalize(P);

	vec2 texture_size = textureSize(color_sampler, int(lod));

	vec3 c = vec3(0,0,0);

	float weight_sum;

	for(int i=0; i<SAMPLES; i++) {
		float r = mix(LAST_SAMPLE ? 0.0 : R/2.0, R, random(vec4(vertex_out.tex_coords, float(i), 0.0)));

		float angle = float(i) / float(SAMPLES) * PI * 2.0;
		float sin_angle = sin(angle);
		float cos_angle = cos(angle);

		vec2 p = vertex_out.tex_coords + vec2(sin(angle), cos(angle)) * r / texture_size;
		if(p.x>=0.0 && p.x<=1.0 && p.y>=0.0 && p.y<=1.0) {
			float weight;
			c += max(vec3(0.0), calc_illumination_from(p, P, N, albedo, F0, roughness, V, weight));
			weight_sum += weight;
		}
	}

	float visibility = 1.0 - (weight_sum / float(SAMPLES));

	return c /* (3.0*PI*R*R/4.0) / float(weight_sum + 0.00001);//*/ * 2.0*PI / (weight_sum + 0.00001);
}

vec3 importance_sample() {
	return vec3(0,0,0); // TODO
}

vec3 calc_illumination_from(vec2 src_point, vec3 shaded_point, vec3 shaded_normal,
                            vec3 shaded_albedo, vec3 shaded_F0, float shaded_roughness,
                            vec3 V, out float weight) {
	float lod = pcs.arguments.x;

	float depth  = textureLod(depth_sampler, src_point, lod).r;
	vec4 mat_data = textureLod(mat_data_sampler, src_point, lod);
	vec3 N = decode_normal(mat_data.rg);

	// TODO: replace with better position reconstruction
	vec4 vr = global_uniforms.inv_pure_proj_mat * vec4(src_point * 2.0 + -1.0 - global_uniforms.camera_offset.xy, 1.0, 1.0);

	vec3 P = vr.xyz / vr.w * depth;//TODO: restore position from depth restore_position(src_point, depth);
	vec3 diff = shaded_point - P;
	float r = length(diff);
	if(r<0.0001) { // ignore too close pixels
		weight = 0.0;
		return vec3(0,0,0);
	}

	float r2 = r*r;
	vec3 dir = diff / r;

//	float r2 = max(1.0, dist*dist*0.001);//r*r;

	vec3 radiance = min(vec3(4,4,4), textureLod(color_sampler, src_point, lod).rgb); // p_i * L_i

	float visibility = 1.0; // v_i

	float NdotL_src = clamp(dot(N, dir), 0.0, 1.0); // cos(θ')
	float NdotL_dst = clamp(dot(shaded_normal, -dir), 0.0, 1.0); // cos(θ)
/*
	vec3 x_i = P - global_uniforms.eye_pos.xyz;
	float x_i_length = length(x_i);
	float cos_alpha = 1.0 / x_i_length; // dot(x_i, {0,0,1})
	float cos_beta  = dot(x_i/x_i_length, N);
	float z = depth;

	float fov_h = global_uniforms.proj_planes.z;
	float fov_v = global_uniforms.proj_planes.w;
	vec2 screen_size = textureSize(color_sampler, 0);//int(lod));


	float dp = 1.0;//pow(2, -2*lod); // TODO: screen differential area
	dp = (3.0*PI*R*R/4.0) / float(SAMPLES) * 10000.0;
	float ds = z*z*4.0 * tan(fov_h) * tan(fov_v) / (screen_size.x*screen_size.y)
	           * (cos_alpha / cos_beta) * dp;

	//r2 = max(0.0, r2*0.0001);
	//ds = 1.0;//0.6; // TODO: ds is ~10^-5 and results in no light at all
	float R2 = 1.0 / PI * NdotL_src * ds;

	float area = (R2 * NdotL_dst) / (r2 + R2); // point-to-differential area form-factor
*/

	// TODO: additional factor to reduce self-lighting artefacts clamp(1.0-dot(N,shaded_normal), 0.0, 1.0)

	// eq. 3 (for testing)
	float area = NdotL_dst*NdotL_src / max(1.0, r2*0.001) * clamp(1.0-dot(N,shaded_normal), 0.0, 1.0);// * ds;

//	float R2 = 1.0 / PI * NdotL_src * (3.0*PI*R*R/4.0) / float(SAMPLES);
//	area = (R2 * NdotL_dst) / (r2 + R2);

	weight = area > 0.0 ? 1.0 : 0.0;

	float perspective_correction = 1.0 + clamp(1.0 - dot(-normalize(P), N), 0, 1)*0.5;

	return radiance * area * perspective_correction;

	// TODO: include specular indirect illumination
	//return radiance * NdotL_dst * visibility * area;
	// return brdf(shaded_albedo, shaded_F0, shaded_roughness, shaded_normal, V, -dir, radiance) * visibility * area;
}

