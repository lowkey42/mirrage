#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "particle_transparent_base.glsl"

layout(location = 5) in vec4 in_shadows;

layout (constant_id = 0) const int FRAGMENT_SHADOWS = 1;

vec3 decode_tangent_normal(vec2 tn) {
	vec3 N = vec3(tn, 0);
	N.z = sqrt(1 - dot(N.xy, N.xy));
	return N;
}

vec3 tangent_space_to_world(vec3 VN, vec3 N) {
	vec3 vp = view_pos.xyz/view_pos.w;

	// calculate tangent
	vec3 p_dx = dFdx(vp);
	vec3 p_dy = dFdy(vp);

	vec2 tc_dx = dFdx(tex_coords);
	vec2 tc_dy = dFdy(tex_coords);

	vec3 p_dy_N = cross(p_dy, VN);
	vec3 p_dx_N = cross(VN, p_dx);

	vec3 T = p_dy_N * tc_dx.x + p_dx_N * tc_dy.x;
	vec3 B = p_dy_N * tc_dx.y + p_dx_N * tc_dy.y;

	float inv_max = inversesqrt(max(dot(T,T), dot(B,B)));
	mat3 TBN = mat3(T*inv_max, B*inv_max, VN);
	return normalize(TBN * N);
}

float sample_shadow(mat4 light_space, int shadowmap, vec3 p) {
	vec4 lightspace_pos = light_space * vec4(p, 1);
	lightspace_pos /= lightspace_pos.w;
	lightspace_pos.xy = lightspace_pos.xy * 0.5 + 0.5;

	if(lightspace_pos.x<0 || lightspace_pos.y<0 || lightspace_pos.x>=1 || lightspace_pos.y>=1)
		return 1.0;
	else {
		return texture(sampler2DShadow(shadowmaps[shadowmap], shadowmap_shadow_sampler),
		               lightspace_pos.xyz);
	}
}

vec3 calc_lighting(vec3 albedo, vec3 radiance, vec3 shadow_radiance, vec3 position, vec3 N, vec3 L,
                   float roughness, float metallic, int shadowmap) {

	const float PI = 3.14159265359;

	vec3 V = -normalize(position);

	vec3 F0 = mix(vec3(0.04), albedo.rgb, metallic);
	albedo.rgb *= 1.0 - metallic;

	float shadow = 1;
	if(FRAGMENT_SHADOWS==1) {
		if(shadowmap>=0) {
			mat4 ls = lights.dir_lights[shadowmap].light_space;
			shadow = sample_shadow(ls, shadowmap, position);
		}
	} else {
		shadow = in_shadows[shadowmap];
	}

	float NdotL = max(dot(N, L), 0.0);

	shadow *= NdotL;

	vec3 out_color = vec3(0,0,0);

	if(shadow>0.0) {
		vec3 diffuse;
		out_color = brdf_without_NdotL(albedo, F0, roughness, N, V, L, radiance, diffuse) * shadow;
	}

	return out_color + shadow_radiance * albedo / PI * (1.0 - shadow);
}

vec4 calc_color(int mip) {
	vec4 albedo = texture(albedo_sampler, tex_coords);
	albedo *= out_particle_color;

	vec2 screen_uv = out_screen_pos.xy/out_screen_pos.w*0.5+0.5;
	float background_depth = textureLod(depth_sampler, screen_uv, mip).r * -global_uniforms.proj_planes.y;
	albedo.a *= smoothstep(0, 0.2, abs(background_depth-view_pos.z/view_pos.w));

	if(albedo.a<0.001) {
		discard;
	}

	vec2 N_tangent = texture(normal_sampler, tex_coords).rg*2-1;
	vec3  N = normalize(normal);
	if(dot(N_tangent,N_tangent) < 0.0001)
		N = tangent_space_to_world(N, decode_tangent_normal(N_tangent));

	vec4 brdf = texture(brdf_sampler, tex_coords);
	float roughness = brdf.r;
	float metallic  = brdf.g;
	roughness = mix(0.01, 0.99, roughness*roughness);

	float emissive_power = texture(emission_sampler, tex_coords).r;

	vec4 result_color = vec4(0,0,0,albedo.a);

	result_color.rgb += albedo.rgb *  model_uniforms.light_data.rgb * emissive_power
	                    * model_uniforms.light_data.a * albedo.a;

	if(lights.count==0) {
		result_color.rgb += albedo.rgb * albedo.a;

	} else {
		for(int i=0; i<lights.count; i++) {
			result_color.rgb += calc_lighting(albedo.rgb,
			                                  lights.dir_lights[i].radiance.rgb, lights.dir_lights[i].shadow_radiance.rgb,
			                                  view_pos.xyz/view_pos.w, N, lights.dir_lights[i].dir.xyz, roughness, metallic,
			                                  i)
			        * albedo.a;
		}
	}

	return result_color;
}
