#ifndef GI_BLEND_COMMON_INCLUDED
#define GI_BLEND_COMMON_INCLUDED

#include "normal_encoding.glsl"
#include "upsample.glsl"
#include "brdf.glsl"
#include "color_conversion.glsl"


vec3 calculate_gi(vec2 uv, vec3 radiance, vec3 specular,
                  sampler2D albedo_sampler, sampler2D mat_sampler, sampler2D brdf_sampler) {
	const float PI = 3.14159265359;

	// load / calculate material properties and factors
	vec3 albedo = textureLod(albedo_sampler, uv, 0.0).rgb;

	vec4 mat_data = textureLod(mat_sampler, uv, 0.0);
	float roughness = mat_data.b;
	float metallic = mat_data.a;

	vec3 F0 = mix(vec3(0.04), albedo.rgb, metallic);
	albedo.rgb *= 1.0 - metallic;

	vec3 N = decode_normal(mat_data.rg);

	float depth  = textureLod(depth_sampler, uv, 0.0).r;
	vec3 P = position_from_ldepth(vertex_out.tex_coords, depth);
	vec3 V = -normalize(P);

	float NdotV = clamp(dot(N, V), 0.0, 1.0);

	vec2 brdf = texture(brdf_sampler, vec2(NdotV, roughness)).rg;

	vec3 diff = albedo * radiance*(1.0 - F0*brdf.x) / PI;
	vec3 spec = specular.rgb * (F0*brdf.x + brdf.y);

	return max(diff + spec, vec3(0,0,0))*1000.0;
}

#endif
