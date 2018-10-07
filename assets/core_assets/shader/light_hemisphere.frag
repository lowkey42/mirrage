#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "global_uniforms.glsl"
#include "normal_encoding.glsl"


layout(location = 0) in Vertex_data {
	vec2 tex_coords;
} vertex_out;

layout(location = 0) out vec4 out_color;
layout(location = 1) out vec4 out_color_diff;

layout(set=1, binding = 0) uniform sampler2D depth_sampler;
layout(set=1, binding = 1) uniform sampler2D mat_data_sampler;
layout(set=1, binding = 2) uniform sampler2D albedo_sampler;
layout(set=1, binding = 3) uniform sampler2D ao_sampler;
layout(set=1, binding = 4) uniform sampler2D brdf_sampler;
layout(set=1, binding = 5) uniform sampler2DShadow shadowmap;

layout(push_constant) uniform Per_model_uniforms {
	mat4 model;
	vec4 light_color;
	vec4 light_data;  // R=src_radius, GBA=direction
	vec4 light_data2; // R=shadowmapID
} model_uniforms;


const float PI = 3.14159265359;


void main() {
	float depth         = subpassLoad(depth_sampler).r;
	vec4  albedo_mat_id = subpassLoad(albedo_mat_id_sampler);
	vec4  mat_data      = subpassLoad(mat_data_sampler);

	vec3 position = position_from_ldepth(vertex_out.tex_coords, depth);
	vec3 V = -normalize(position);
	vec3 albedo = albedo_mat_id.rgb;

	vec3 N = decode_normal(mat_data.rg);
	float roughness = mat_data.b;
	float metallic = mat_data.a;

	vec3 F0 = mix(vec3(0.04), albedo.rgb, metallic);
	albedo.rgb *= 1.0 - metallic;
	vec3 radiance = model_uniforms.light_color.rgb * model_uniforms.light_color.a;

	vec4 lightspace_pos = model_uniforms.model * vec4(view_pos, 1.0);
	lightspace_pos /= lightspace_pos.w;
	lightspace_pos.xy = clamp(lightspace_pos.xy * 0.5 + 0.5, vec2(0), vec2(1));

	// TODO: sampling pattern
	float visiblity = 1.0;
	for (int i=0;i<min(samples, 16);i++) {
		vec2 point = samples <= 8 ? (samples <= 4 ? Poisson4[i] : Poisson8[i]) : Poisson16[i];

		vec2 offset = vec2(point.x*cos_angle - point.y*sin_angle, point.x*sin_angle + point.y*cos_angle);

		vec2 p = lightspace_pos.xy + offset * sample_size;

		visiblity -= 1.0/samples * (1.0 - texture(sampler2DShadow(shadowmaps[shadowmap], shadowmap_shadow_sampler),
		                                     vec3(p, lightspace_pos.z-z_bias)));
	}

	out_color = vec4(0,0,0,0);
	out_color_diff = vec4(0,0,0,0);

	if(visiblity>0.0) {
		vec3 L = model_uniforms.light_data.gba;

		float weight = 0.5 * dot(N, L) + 0.5;
		vec2 brdf = texture(brdf_sampler, vec2(clamp(dot(N, V), 0.0, 1.0), roughness)).rg;

		vec3 light_color = weight*visiblity*radiance;
		vec3 diff = light_color * albedo * (1.0 - F0*brdf.x) / PI;
		vec3 spec = light_color * (F0*brdf.x + brdf.y);

		out_color = vec4(diff+spec, 1.0);
		out_color_diff = vec4(diff, 1.0);
	}
}
