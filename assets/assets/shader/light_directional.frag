#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "normal_encoding.glsl"
#include "poisson.glsl"
#include "random.glsl"


layout(location = 0) in Vertex_data {
	vec2 tex_coords;
} vertex_out;

layout(location = 0) out vec4 out_color;

layout(input_attachment_index = 0, set=1, binding = 0) uniform subpassInput depth_sampler;
layout(input_attachment_index = 1, set=1, binding = 1) uniform subpassInput albedo_mat_id_sampler;
layout(input_attachment_index = 2, set=1, binding = 2) uniform subpassInput mat_data_sampler;

layout(set=2, binding = 0) uniform texture2D shadowmaps[1];
layout(set=2, binding = 1) uniform samplerShadow shadowmap_shadow_sampler; // sampler2DShadow
layout(set=2, binding = 2) uniform sampler shadowmap_depth_sampler; // sampler2D

layout (constant_id = 0) const int SHADOW_QUALITY = 1;

layout(binding = 0) uniform Global_uniforms {
	mat4 view_proj;
	mat4 inv_view_proj;
	vec4 eye_pos;
	vec4 proj_planes;
	vec4 time;
} global_uniforms;

layout(push_constant) uniform Per_model_uniforms {
	mat4 model;
	vec4 light_color;
	vec4 light_data;  // R=src_radius, GBA=direction
	vec4 light_data2; // R=shadowmapID
} model_uniforms;


const float PI = 3.14159265359;

// TODO: refactor and rewrite as required
float DistributionGGX(vec3 N, vec3 H, float roughness) {
	float a      = roughness*roughness;
	float a2     = a*a;
	float NdotH  = max(dot(N, H), 0.0);
	float NdotH2 = NdotH*NdotH;

	float nom   = a2;
	float denom = (NdotH2 * (a2 - 1.0) + 1.0);
	denom = PI * denom * denom;

	return nom / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness) {
	float r = (roughness + 1.0);
	float k = (r*r) / 8.0;

	float nom   = NdotV;
	float denom = NdotV * (1.0 - k) + k;

	return nom / denom;
}
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
	float NdotV = max(dot(N, V), 0.0);
	float NdotL = max(dot(N, L), 0.0);
	float ggx2  = GeometrySchlickGGX(NdotV, roughness);
	float ggx1  = GeometrySchlickGGX(NdotL, roughness);

	return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
	return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}
// END TODO

float sample_shadowmap(vec3 world_pos);

float linearize_depth(float depth) {
	float near_plane = global_uniforms.proj_planes.x;
	float far_plane = global_uniforms.proj_planes.y;
	return (2.0 * near_plane)
	     / (far_plane + near_plane - depth * (far_plane - near_plane));
}

vec3 restore_position(vec2 uv, float depth) {
	vec4 pos_world = global_uniforms.inv_view_proj * vec4(uv * 2.0 - 1.0, depth, 1.0);
	return pos_world.xyz / pos_world.w;
}


void main() {
	float depth         = subpassLoad(depth_sampler).r;
	vec4  albedo_mat_id = subpassLoad(albedo_mat_id_sampler);
	vec4  mat_data      = subpassLoad(mat_data_sampler);

	float linear_depth = linearize_depth(depth);
	vec3 position = restore_position(vertex_out.tex_coords, depth);
	vec3 V = normalize(global_uniforms.eye_pos.xyz - position);
	vec3 albedo = albedo_mat_id.rgb;
	int  material = int(albedo_mat_id.a*255);

	// material 255 (unlit)
	if(material==255) {
		out_color = vec4(albedo, 1.0);
		return;
	}

	// material 0 (default)
	vec3 N = decode_normal(mat_data.rg);
	float roughness = mat_data.b;
	float metallic = mat_data.a;

	vec3 F0 = mix(vec3(0.04), albedo.rgb, metallic);
	albedo.rgb *= 1.0 - metallic;
	vec3 radiance = model_uniforms.light_color.rgb * model_uniforms.light_color.a;


	float shadow = sample_shadowmap(position + N*0.4);

	out_color = vec4(0,0,0,0);

	if(shadow>0.0) {
		// TODO: calc light

		vec3 L = model_uniforms.light_data.gba;

		vec3 H = normalize(V+L);

		float NDF = DistributionGGX(N, H, roughness);
		float G   = GeometrySmith(N, V, L, roughness);
		vec3 F    = fresnelSchlick(max(dot(H, L), 0.0), F0);

		vec3 kS = F;
		vec3 kD = vec3(1.0) - kS;

		vec3 nominator    = NDF * G * F;
		float denominator = 4 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.001;
		vec3 brdf = nominator / denominator;

		// add to outgoing radiance Lo
		float NdotL = max(dot(N, L), 0.0);
		out_color = vec4((kD * albedo / PI + brdf) * radiance * NdotL * shadow, 1.0);
	}

	// TODO: remove ambient
	out_color.rgb += albedo * radiance * 0.003 + F0*radiance*0.001;
}


float calc_penumbra(vec3 surface_lightspace, float light_size,
                    out int num_occluders);

float sample_shadowmap(vec3 world_pos) {
	int shadowmap = int(model_uniforms.light_data2.r);
	if(shadowmap<0)
		return 1.0;

	vec4 lightspace_pos = model_uniforms.model * vec4(world_pos, 1.0);
	lightspace_pos /= lightspace_pos.w;
	lightspace_pos.xy = lightspace_pos.xy * 0.5 + 0.5;

	float shadowmap_size = textureSize(sampler2D(shadowmaps[shadowmap], shadowmap_depth_sampler), 0).x;
	float light_size = model_uniforms.light_data.r / 800.0;

	int num_occluders;
	float penumbra_softness = calc_penumbra(lightspace_pos.xyz, light_size, num_occluders);

	//return penumbra_softness>=0.5 ? 1.0 : 0.0;

	if(num_occluders==0)
		return 1.0;

	float sample_size = mix(1.0/shadowmap_size, light_size, penumbra_softness);
	int samples = int(mix(4, SHADOW_QUALITY<=1 ? 8 : 16, penumbra_softness));
	if(num_occluders>=4)
		samples = min(samples, 8);

	if(SHADOW_QUALITY<=1)
		samples = min(samples, 8);

	float z_bias = 0.002;

	float angle = random(vec4(world_pos, global_uniforms.time.y));
	float sin_angle = sin(angle);
	float cos_angle = cos(angle);

	float visiblity = 1.0;
	for (int i=0;i<min(samples, 16);i++) {
		vec2 point = samples <= 8 ? (samples <= 4 ? Poisson4[i] : Poisson8[i]) : Poisson16[i];

		vec2 offset = vec2(point.x*cos_angle - point.y*sin_angle, point.x*sin_angle + point.y*cos_angle);

		vec2 p = lightspace_pos.xy + offset * sample_size;

		visiblity -= 1.0/samples * (1.0 - texture(sampler2DShadow(shadowmaps[shadowmap], shadowmap_shadow_sampler),
		                                     vec3(p, lightspace_pos.z-z_bias)));
	}

	return clamp(visiblity, 0.0, 1.0);
}

float calc_avg_occluder(vec3 surface_lightspace, float search_area,
                        out int num_occluders) {
	int shadowmap = int(model_uniforms.light_data2.r);

	float depth_acc;
	float depth_count;
	num_occluders = 0;
#
	float angle = random(vec4(surface_lightspace, global_uniforms.time.y));
	float sin_angle = sin(angle);
	float cos_angle = cos(angle);

	for (int i=0;i<4;i++) {
		vec2 offset = vec2(Poisson4[i].x*cos_angle - Poisson4[i].y*sin_angle, Poisson4[i].x*sin_angle + Poisson4[i].y*cos_angle);
		
		float depth = texture(sampler2D(shadowmaps[shadowmap], shadowmap_depth_sampler),
		                      surface_lightspace.xy + offset * search_area).r;
		if(depth < surface_lightspace.z - 0.002) {
			depth_acc += depth;
			depth_count += 1.0;
			num_occluders += 1;
		}
	}

	if(num_occluders==0)
		return 1.0;

	return depth_acc / depth_count;
}

float calc_penumbra(vec3 surface_lightspace, float light_size, out int num_occluders) {
	float avg_occluder = calc_avg_occluder(surface_lightspace, light_size, num_occluders);

	const float scale = 8.0;
	float softness = (surface_lightspace.z - avg_occluder) * scale;

	return smoothstep(0.2, 1.0, softness);
}
