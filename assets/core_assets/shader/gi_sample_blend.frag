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

layout (constant_id = 0) const int INCLUDE_AO = 0;  // 1 if the result should be modulated by AO

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

// calculate luminance of a color (used for normalization)
float luminance_norm(vec3 c) {
	vec3 f = vec3(0.299,0.587,0.114);
	return sqrt(c.r*c.r*f.r + c.g*c.g*f.g + c.b*c.b*f.b);
}

vec3 clip_aabb(vec3 aabb_min, vec3 aabb_max, vec3 p, vec3 q) {
	// note: only clips towards aabb center (but fast!)
	vec3 p_clip = 0.5 * (aabb_max + aabb_min);
	vec3 e_clip = 0.5 * (aabb_max - aabb_min) + 0.00000001;

	vec3 v_clip = q - p_clip;
	vec3 v_unit = v_clip.xyz / e_clip;
	vec3 a_unit = abs(v_unit);
	float ma_unit = max(a_unit.x, max(a_unit.y, a_unit.z));

	if (ma_unit > 1.0)
		return p_clip + v_clip / ma_unit;
	else
		return q;// point inside aabb
}

float pixel_intensity(vec3 c) {
	return dot(c,c);
	//return length(c);
}

// A Fast, Small-Radius GPU Median Filter by Morgan McGuire: http://casual-effects.com/research/McGuire2008Median/index.html
#define s2(a, b)				temp = a; a = min(a, b); b = max(temp, b);
#define mn3(a, b, c)			s2(a, b); s2(a, c);
#define mx3(a, b, c)			s2(b, c); s2(a, c);

#define mnmx3(a, b, c)			mx3(a, b, c); s2(a, b);                                   // 3 exchanges
#define mnmx4(a, b, c, d)		s2(a, b); s2(c, d); s2(a, c); s2(b, d);                   // 4 exchanges
#define mnmx5(a, b, c, d, e)	s2(a, b); s2(c, d); mn3(a, c, e); mx3(b, d, e);           // 6 exchanges
#define mnmx6(a, b, c, d, e, f) s2(a, d); s2(b, e); s2(c, f); mn3(a, b, c); mx3(d, e, f); // 7 exchanges

void main() {
	// read diffuse color, modulate with modulo if equal to min/max of neighborhood => noise
	ivec2 result_sampler_size = textureSize(result_sampler, 0).xy;
	ivec2 uv = ivec2(result_sampler_size * vertex_out.tex_coords);
	vec3 colors[9];
	for(int x=-1; x<=1; x++) {
		for(int y=-1; y<=1; y++) {
			ivec2 c_uv = uv+ivec2(x,y);
			c_uv = clamp(c_uv, ivec2(0,0), result_sampler_size-ivec2(0,0));
			colors[(x+1)*3+(y+1)] = texelFetch(result_sampler, c_uv, 0).rgb;
		}
	}

	float min_c = pixel_intensity(colors[0]);
	float max_c = min_c;

	for(int i=1; i<9; i++) {
		float intensity = pixel_intensity(colors[i]);
		min_c = min(min_c, intensity);
		max_c = max(max_c, intensity);
	}

	vec3 org = colors[4];
	float org_intensity = dot(org, org);
	if(min_c<org_intensity && max_c>org_intensity)
		out_color = vec4(org, 1.0);
	else {
		// Starting with a subset of size 6, remove the min and max each time
		vec3 temp;
		mnmx6(colors[0], colors[1], colors[2], colors[3], colors[4], colors[5]);
		mnmx5(colors[1], colors[2], colors[3], colors[4], colors[6]);
		mnmx4(colors[2], colors[3], colors[4], colors[7]);
		mnmx3(colors[3], colors[4], colors[8]);

		out_color = vec4(colors[4], 1.0);
	}

	// modulate diffuse GI by ambient occlusion
	float ao = 1.0;
	if(INCLUDE_AO==1 && pcs.projection[3][3]>0.0) {
		ao = texture(ao_sampler, vertex_out.tex_coords).r;
		ao = mix(1.0, ao, pcs.projection[3][3]);
	}
	out_color.rgb *= ao;
	for(int i=0; i<9; i++)
		colors[i] *= ao;


	// blend with history
	vec3 c_history = texture(history_result_sampler, vertex_out.tex_coords).rgb;

	vec2 texel_size = 1.0 / textureSize(result_sampler, 0);

	vec2 du = vec2(texel_size.x, 0.0);
	vec2 dv = vec2(0.0, texel_size.y);

	vec3 ctl = colors[0];
	vec3 ctc = colors[1];
	vec3 ctr = colors[2];
	vec3 cml = colors[3];
	vec3 cmc = out_color.rgb;
	vec3 cmr = colors[5];
	vec3 cbl = colors[6];
	vec3 cbc = colors[7];
	vec3 cbr = colors[8];

	vec3 cmin = min(ctl, min(ctc, min(ctr, min(cml, min(cmc, min(cmr, min(cbl, min(cbc, cbr))))))));
	vec3 cmax = max(ctl, max(ctc, max(ctr, max(cml, max(cmc, max(cmr, max(cbl, max(cbc, cbr))))))));

	vec3 cavg = (ctl + ctc + ctr + cml + cmc + cmr + cbl + cbc + cbr) / 9.0;

	vec3 cmin5 = min(ctc, min(cml, min(cmc, min(cmr, cbc))));
	vec3 cmax5 = max(ctc, max(cml, max(cmc, max(cmr, cbc))));
	vec3 cavg5 = (ctc + cml + cmc + cmr + cbc) / 5.0;
	cmin = 0.5 * (cmin + cmin5);
	cmax = 0.5 * (cmax + cmax5);
	cavg = 0.5 * (cavg + cavg5);

	vec3 d = cmax - cmin;
	cmin -= d * 0.5;
	cmax += d * 0.5;

	c_history = clip_aabb(cmin.xyz, cmax.xyz, clamp(cavg, cmin, cmax), c_history);

	float weight = texture(history_weight_sampler, vertex_out.tex_coords).g;
	if(dot(c_history,c_history) < dot(out_color.rgb,out_color.rgb))
		weight *= 0.98;
	else
		weight *= 0.8;

	out_color.rgb = mix(out_color.rgb, c_history, weight);
}
