#ifndef GI_BLEND_COMMON_INCLUDED
#define GI_BLEND_COMMON_INCLUDED

#include "normal_encoding.glsl"
#include "upsample.glsl"
#include "brdf.glsl"
#include "color_conversion.glsl"


vec3 calculate_gi(vec2 uv, vec3 radiance, vec3 specular,
                  sampler2D albedo_sampler, sampler2D mat_sampler, sampler2D brdf_sampler,
                  out vec3 diffuse) {
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
	vec3 P = depth * vertex_out.view_ray;
	vec3 V = -normalize(P);
	vec3 L =  -reflect(V, N);
	vec3 H = normalize(V+L);

	float NdotV = clamp(dot(N, V), 0.0, 1.0);
	float HdotL = clamp(dot(H, L), 0.0, 1.0);

	vec2 brdf = texture(brdf_sampler, vec2(NdotV, roughness)).rg;

	vec3 F = fresnelSchlick(HdotL, F0);

	vec3 diff = albedo * radiance*(1.0 - F) / PI;
	vec3 spec = specular.rgb * (F*brdf.x + brdf.y);

	diffuse = albedo * radiance/PI + F0*radiance / (2*PI*PI);

	return clamp(diff + spec, vec3(0,0,0), vec3(10,10,10));
}

vec3 calculate_gi(vec2 uv, vec2 gi_uv, int gi_lod, sampler2D diff_sampler, sampler2D spec_sampler,
                  sampler2D albedo_sampler, sampler2D mat_sampler, sampler2D brdf_sampler,
                  out vec3 diffuse) {
    // load diff + spec GI
	vec3 radiance = upsampled_result(diff_sampler, 0, gi_lod, gi_uv, 0.5).rgb;
	vec3 specular = upsampled_result(spec_sampler, 0,      0, gi_uv, 2.0).rgb;

    return calculate_gi(uv, radiance, specular, albedo_sampler, mat_sampler, brdf_sampler, diffuse);
}

#endif
