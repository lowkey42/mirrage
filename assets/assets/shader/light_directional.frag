#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "normal_encoding.glsl"


layout(location = 0) in Vertex_data {
	vec2 tex_coords;
} vertex_out;

layout(location = 0) out vec4 out_color;

layout(input_attachment_index = 0, set=1, binding = 0) uniform subpassInput depth_sampler;
layout(input_attachment_index = 1, set=1, binding = 1) uniform subpassInput albedo_mat_id_sampler;
layout(input_attachment_index = 2, set=1, binding = 2) uniform subpassInput mat_data_sampler;

layout(binding = 0) uniform Global_uniforms {
	mat4 view_proj;
	mat4 inv_view_proj;
	vec4 eye_pos;
	vec4 proj_planes;
} global_uniforms;

layout(push_constant) uniform Per_model_uniforms {
	mat4 model;
	vec4 light_color;
	vec4 light_data;
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


float linearize_depth(float depth) {
	float near_plane = global_uniforms.proj_planes.x;
	float far_plane = global_uniforms.proj_planes.y;
	float z = depth * 2.0 - 1.0; // to NDC // TODO: still required? / test
	return (2.0 * near_plane * far_plane)
	     / (far_plane + near_plane - z * (far_plane - near_plane));
}

vec3 restore_position(vec2 uv, float depth) {
	vec4 pos_world = global_uniforms.inv_view_proj * vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
	return pos_world.xyz / pos_world.w;
}


void main() {
	float depth         = subpassLoad(depth_sampler).r;
	vec4  albedo_mat_id = subpassLoad(albedo_mat_id_sampler);
	vec4  mat_data      = subpassLoad(mat_data_sampler);

	float linear_depth = linearize_depth(depth);
	vec3 position = restore_position(vertex_out.tex_coords, linear_depth);
	vec3 V = normalize(global_uniforms.eye_pos.xyz - position);
	vec3 albedo = albedo_mat_id.rgb;
	int  material = int(albedo_mat_id.a*255);

	// material 0 (default)
	vec3 N = decode_normal(mat_data.rg);
	float roughness = mat_data.b;
	float metallic = mat_data.a;

	// TODO: calc light

	vec3 F0 = mix(vec3(0.04), albedo.rgb, metallic);
	albedo.rgb *= 1.0 - metallic;

	vec3 radiance = vec3(8.0, 8.0, 7.0);
	vec3 L = normalize(vec3(0.4,1,0.1));

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
	out_color = vec4((kD * albedo / PI + brdf) * radiance * NdotL, 1.0);
}
