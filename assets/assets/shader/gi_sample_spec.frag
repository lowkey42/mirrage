#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable


layout(location = 0) in Vertex_data {
	vec2 tex_coords;
	vec3 view_ray;
	flat vec3 corner_view_rays[4];
} vertex_out;

layout(location = 0) out vec4 out_color;

layout(set=1, binding = 0) uniform sampler2D color_sampler;
layout(set=1, binding = 1) uniform sampler2D depth_sampler;
layout(set=1, binding = 2) uniform sampler2D mat_data_sampler;
layout(set=1, binding = 3) uniform sampler2D result_sampler;
layout(set=1, binding = 5) uniform sampler2D albedo_sampler;

layout (constant_id = 0) const bool LAST_SAMPLE = false;
layout (constant_id = 1) const float R = 40;
layout (constant_id = 2) const int SAMPLES = 128;
layout (constant_id = 3) const int MAX_RAYCAST_STEPS = 32;

layout(push_constant) uniform Push_constants {
	mat4 projection;
	vec4 arguments;
} pcs;


#include "global_uniforms.glsl"
#include "normal_encoding.glsl"
#include "random.glsl"
#include "brdf.glsl"
#include "upsample.glsl"
#include "raycast.glsl"

void main() {
	out_color = vec4(0,0,0,1);

	float depth  = textureLod(depth_sampler, vertex_out.tex_coords, 0.0).r;
	vec3 P = depth * vertex_out.view_ray;

	vec4 mat_data = textureLod(mat_data_sampler, vertex_out.tex_coords, 0.0);
	vec3 N = decode_normal(mat_data.rg);

	vec3 V = -normalize(P);

	vec3 dir = -reflect(V, N);

	vec2 raycast_hit_uv;
	vec3 raycast_hit_point;
	if(traceScreenSpaceRay1(P+dir, dir, pcs.projection, depth_sampler,
							textureSize(depth_sampler, 0), 1.0, global_uniforms.proj_planes.x,
							max(1, 1), 0.1, 128, 20.0, 0,
							raycast_hit_uv, raycast_hit_point)) {

		vec3 radiance = texelFetch(color_sampler, ivec2(raycast_hit_uv), 0).rgb;

		float roughness = mat_data.b;

		vec3 L = raycast_hit_point - P;
		float L_length = length(L);
		float attenuation = 1.0 / max(1.0, L_length * L_length);
		L /= L_length;

		const float PI = 3.14159265359;

		vec3 H = normalize(V+L);

		float NDF = DistributionGGX(N, H, roughness);
		float G   = GeometrySmith(N, V, L, roughness);

		float nominator   = NDF * G;
		float denominator = 4 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.001;

		out_color.rgb = nominator / denominator * radiance * attenuation;
		out_color.a = max(dot(H, L), 0.0);
	}
}

