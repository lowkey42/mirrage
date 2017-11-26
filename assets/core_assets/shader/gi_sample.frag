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
layout(set=1, binding = 7) uniform sampler2D ao_sampler;

layout (constant_id = 0) const int LAST_SAMPLE = 0;  // 1 if this is the last MIP level to sample
layout (constant_id = 1) const float R = 40;         // the radius to fetch samples from
layout (constant_id = 2) const int SAMPLES = 128;    // the number of samples to fetch
layout (constant_id = 3) const int UPSAMPLE_ONLY = 0;// 1 if only the previous result should be
                                                     //   upsampled but no new samples calculated

// nearer samples have a higher weight. Less physically correct but results in more noticable color bleeding
layout (constant_id = 4) const float PRIORITISE_NEAR_SAMPLES = 0.6;

// arguments are packet into the matrices to keep the pipeline layouts compatible between GI passes
layout(push_constant) uniform Push_constants {
	// [3][3] = intensity of ambient occlusion (0 = disabled)
	mat4 projection;

	// [0][0] = higher resolution base MIP level
	// [2][0] = exponent for alternative MIP level scaling factor (see PRIORITISE_NEAR_SAMPLES)
	// [0][3] = current MIP level (relative to base MIP)
	// [1][3] = highest relevant MIP level (relative to base MIP)
	// [2][3] = precalculated part of the ds factor used in calc_illumination_from
	// [3][3] = base MIP level
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

// calculate luminance of a color (used for normalization)
float luminance_norm(vec3 c) {
	vec3 f = vec3(0.299,0.587,0.114);
	return sqrt(c.r*c.r*f.r + c.g*c.g*f.g + c.b*c.b*f.b);
}

void main() {
	float current_mip = pcs.prev_projection[0][3];
	float max_mip     = pcs.prev_projection[1][3];
	float base_mip    = pcs.prev_projection[3][3];

	// upsample the previous result (if there is one)
	if(current_mip < max_mip)
		out_color = vec4(upsampled_result(depth_sampler, mat_data_sampler,
		                                  prev_depth_sampler, prev_mat_data_sampler,
		                                  result_sampler, vertex_out.tex_coords), 1.0);
	else
		out_color = vec4(0,0,0, 1);

	// calculate contibution from this level (if we haven't reached the target level, yet)
	if(UPSAMPLE_ONLY==0)
		out_color.rgb += gi_sample(int(current_mip+0.5), int(base_mip+0.5));

	// reached the last MIP level => blend with history
	if(abs(current_mip - base_mip) < 0.00001) {
		// calculate interpolation factor based on the depth-error in its surrounding during reporjection
		vec2 hws_step = 1.0 / textureSize(history_weight_sampler, 0);

		vec4  history_weights = textureGather(history_weight_sampler, vertex_out.tex_coords+hws_step, 0);
		float history_weight  = min(history_weights.x, min(history_weights.y, min(history_weights.z, history_weights.w)));

		history_weights = textureGather(history_weight_sampler, vertex_out.tex_coords-hws_step, 0);
		history_weight  = min(history_weight,min(history_weights.x, min(history_weights.y, min(history_weights.z, history_weights.w))));

		// modulate diffuse GI by ambient occlusion
		if(pcs.projection[3][3]>0.0) {
			float ao = texture(ao_sampler, vertex_out.tex_coords).r;
			ao = mix(1.0, ao, pcs.projection[3][3]);
			out_color.rgb *= ao;
		}

		// normalize diffuse GI by its luminance to reduce fire-flies
		out_color.rgb /= (1 + luminance_norm(out_color.rgb));

		// calculate the min/max interpolation weights based on the delta time
		float weight_measure = smoothstep(1.0/120.0, 1.0/30.0, global_uniforms.time.z);
		float weight_min = mix(0.85, 0.1, weight_measure);
		float weight_max = mix(0.98, 0.85, weight_measure);

		// scale by calculated weight to alpha-blend with the reprojected result of the previous frame
		out_color *= 1.0 - mix(weight_min, weight_max, history_weight);
	}

	// clamp the result to a reasonable range to reduce artefacts
	out_color = clamp(out_color, vec4(0), vec4(20));
}

const float PI     = 3.14159265359;
const float REC_PI = 0.3183098861837906715; // 1/PI

// calculates the diffuse GI contribution from this MIP level
vec3 gi_sample(int lod, int base_mip) {
	// calculate uv coordinates in pixel
	vec2 texture_size = textureSize(color_sampler, 0);
	ivec2 uv = ivec2(vertex_out.tex_coords * texture_size);

	// clamp area to reduce artefacts around borders
	if(uv.y >= texture_size.y-1 || uv.x >= texture_size.x-1)
		return vec3(0, 0, 0);

	// fetch the depth/normal of the target pixel and reconstruct its view-space position
	float depth  = texelFetch(depth_sampler, uv, 0).r;
	vec4 mat_data = texelFetch(mat_data_sampler, uv, 0);
	vec3 N = decode_normal(mat_data.rg);
	vec3 P = position_from_ldepth(vertex_out.tex_coords, depth);

	// fetch SAMPLES samples in a spiral pattern and combine their GI contribution
	vec3 c = vec3(0,0,0);
	float samples_used = 0.0;
	float angle = random(vec4(vertex_out.tex_coords, 0, global_uniforms.time.x*10.0));
	float angle_step = 1.0 / float(SAMPLES) * PI * 2.0 * 19.0;

	for(int i=0; i<SAMPLES; i++) {
		float r = max(4, mix(LAST_SAMPLE==1 ? 4.0 : R/2.0, R, float(i)/float(SAMPLES)));

		angle += angle_step;
		float sin_angle = sin(angle);
		float cos_angle = cos(angle);

		ivec2 p = ivec2(uv + vec2(sin_angle, cos_angle) * r);
		float weight;
		vec3 sc = calc_illumination_from(lod, texture_size, p, uv, depth, P, N, weight);

		c += sc;
		samples_used += weight;
	}

	// scale the collected diffuse GI by a factor to compensate for low intensity of smaller MIP levels
	float actual_lod = lod - pcs.prev_projection[0][0];
	float scale_exponent = mix(actual_lod,
	                           pcs.prev_projection[2][0],
	                           PRIORITISE_NEAR_SAMPLES);
	c *= pow(2.0, scale_exponent*2);

	// normalize based on number of samples for consistent results, independent of SAMPLES
	c *= 128.0 / SAMPLES;

	return c;
}

// calculate the light transfer between two pixel of the current level
vec3 calc_illumination_from(int lod, vec2 tex_size, ivec2 src_uv, vec2 shaded_uv, float shaded_depth,
                            vec3 shaded_point, vec3 shaded_normal, out float weight) {
	// fetch depth/normal at src pixel
	vec4 mat_data = texelFetch(mat_data_sampler, src_uv, 0);
	vec3 N = decode_normal(mat_data.rg);
	float depth  = texelFetch(depth_sampler, src_uv, 0).r;

	if(depth>=0.9999) {
		// we hit the skybox => reduce depth so it still contributes some light
		depth = 0.1;
	}

	// reconstruct the position (x_i) of the src point and calculate the direction and distance^2 to x
	vec3 P = position_from_ldepth(src_uv / tex_size, depth);
	vec3 Pn = normalize(P);
	vec3 diff = shaded_point - P;
	vec3 dir = normalize(diff);
	float r2 = dot(diff, diff);

	float visibility = 1.0; // v(x, x_i); currently not implemented

	// interpolate r^2 to (r^2)^1.25 for distant pixels to counter missing visibility term
	r2 = mix(r2, pow(r2,1.25), clamp((r2-1)*0.5, 0, 1));

	float NdotL_src = clamp(dot(N, dir),              0.0, 1.0); // cos(θ')
	float NdotL_dst = clamp(dot(shaded_normal, -dir), 0.0, 1.0); // cos(θ)

	// if the material is an emitter (mat_data.b=0), flip the normal if that would result in a higher
	//   itensity (approximates light emitted from the backside by assuming rotation invariance)
	NdotL_src = mix(max(clamp(dot(-N, dir), 0.0, 1.0), NdotL_src), NdotL_src, step(0.0001, mat_data.b));

	// calculate the size of the differential area
	float cos_alpha = Pn.z;
	float cos_beta  = dot(Pn, N);
	float z = depth * global_uniforms.proj_planes.y;
	float ds = pcs.prev_projection[2][3] * z*z * clamp(cos_alpha / cos_beta, 0.001, 1000.0);

	// multiply all factors, that modulate the light transfer
	weight = visibility * NdotL_dst * NdotL_src * ds / (0.1+r2);

	// fetch the light emitted by the src pixel, modulate it by the calculated factor and return it
	vec3 radiance = texelFetch(color_sampler, src_uv, 0).rgb;
	return max(vec3(0.0), radiance * weight);
}

