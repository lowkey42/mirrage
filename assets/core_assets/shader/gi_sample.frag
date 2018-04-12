#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable


layout(location = 0) in Vertex_data {
	vec2 tex_coords;
} vertex_out;

layout(location = 0) out vec4 out_color;

layout(set=1, binding = 0) uniform sampler2D noise_sampler;

layout(set=2, binding = 0) uniform sampler2D color_sampler;
layout(set=2, binding = 1) uniform sampler2D depth_sampler;
layout(set=2, binding = 2) uniform sampler2D mat_data_sampler;
layout(set=2, binding = 3) uniform sampler2D prev_level_result_sampler;
layout(set=2, binding = 4) uniform sampler2D result_sampler;
layout(set=2, binding = 5) uniform sampler2D history_result_sampler;
layout(set=2, binding = 6) uniform sampler2D history_weight_sampler;
layout(set=2, binding = 7) uniform sampler2D prev_depth_sampler;
layout(set=2, binding = 8) uniform sampler2D prev_mat_data_sampler;
layout(set=2, binding = 9) uniform sampler2D ao_sampler;

layout (constant_id = 0) const int LAST_SAMPLE = 0;  // 1 if this is the last MIP level to sample
layout (constant_id = 1) const float R = 40;         // the radius to fetch samples from
layout (constant_id = 2) const int SAMPLES = 128;    // the number of samples to fetch


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
                            vec3 shaded_point, vec3 shaded_normal);

// calculate luminance of a color (used for normalization)
float luminance_norm(vec3 c) {
	vec3 f = vec3(0.299,0.587,0.114);
	return sqrt(c.r*c.r*f.r + c.g*c.g*f.g + c.b*c.b*f.b);
}

void main() {
	float current_mip = pcs.prev_projection[0][3];
	float max_mip     = pcs.prev_projection[1][3];
	float base_mip    = pcs.prev_projection[3][3];

	out_color = vec4(0,0,0, 1);

	// calculate contribution from this level
	out_color = vec4(gi_sample(int(current_mip+0.5), int(base_mip+0.5)), 1);

	// clamp the result to a reasonable range to reduce artefacts
	out_color.rgb = clamp(out_color.rgb, vec3(0), vec3(20));

	// normalize diffuse GI by its luminance to reduce fire-flies
	out_color.rgb /= (1 + luminance_norm(out_color.rgb));
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

	float angle =  PDnrand2(vec4(vertex_out.tex_coords, lod, global_uniforms.time.x*10.0)).r * 2*PI;

	float outer_radius = R;
	float inner_radius = LAST_SAMPLE==0 ? outer_radius / 2.0 - 4.0 : 0.0;
	float angle_step = PI * 2.0 / pow((sqrt(5.0) + 1.0) / 2.0, 2.0);

	float normal_dir = 0.5*PI;
	float normal_weight = abs(N.z);
	if(normal_weight<=0.999) {
		vec2 N_2d = normalize(N.xy);
		normal_dir += atan(N_2d.y, N_2d.x);
	}

	for(int i=0; i<SAMPLES; i++) {
		float r = max(
				4.0,
				mix(inner_radius, outer_radius, sqrt(float(i) / float(SAMPLES))));
		float a = i * angle_step + angle;

		a = mod(a, 2.0*PI);
		float a_rel = a / (2.0*PI);

		float d = a_rel<=0.5 ? 2*a_rel : (1-a_rel)*2;
		d = 2.5*d - 5.42*d*d + 3.6*d*d*d + 0.1;
		d += (1-d) * normal_weight;

		if(a_rel<0.5) a = mix(0, a, d);
		else          a = mix(a, 2*PI, 1-d);

		a += normal_dir;

		float sin_angle = sin(a);
		float cos_angle = cos(a);

		ivec2 p = ivec2(uv + vec2(sin_angle * r, cos_angle * r));
		float weight;
		c += calc_illumination_from(lod, texture_size, p, uv, depth, P, N);
	}

	return c;
}

// calculate the light transfer between two pixel of the current level
vec3 calc_illumination_from(int lod, vec2 tex_size, ivec2 src_uv, vec2 shaded_uv, float shaded_depth,
                            vec3 shaded_point, vec3 shaded_normal) {
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

	float NdotL_src = clamp(dot(N, dir),              0.0, 1.0); // cos(θ')
	float NdotL_dst = clamp(dot(shaded_normal, -dir), 0.0, 1.0); // cos(θ)

	// if the material is an emitter (mat_data.b=0), flip the normal if that would result in a higher
	//   itensity (approximates light emitted from the backside by assuming rotation invariance)
	NdotL_src = mix(max(clamp(dot(-N, dir), 0.0, 1.0), NdotL_src), NdotL_src, step(0.0001, mat_data.b));

	// calculate the size of the differential area
	float cos_alpha = Pn.z;
	float cos_beta  = dot(Pn, N);
	float ds = pcs.prev_projection[2][3] * depth*depth * clamp(cos_alpha / cos_beta, 0.001, 1000.0);

	// multiply all factors, that modulate the light transfer
	float weight = visibility * NdotL_dst * NdotL_src * ds / (0.001+r2);

	if(weight<0.001) {
		return vec3(0);

	} else {
		// fetch the light emitted by the src pixel, modulate it by the calculated factor and return it
		vec3 radiance = texelFetch(color_sampler, src_uv, 0).rgb;
		return radiance * weight;
	}
}

