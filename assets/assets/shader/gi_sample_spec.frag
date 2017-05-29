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
	float startLod = pcs.arguments.x;
	vec2 textureSize = textureSize(depth_sampler, int(startLod));

	out_color = vec4(0,0,0,0);

	float depth  = textureLod(depth_sampler, vertex_out.tex_coords, startLod).r;
	vec3 P = depth * vertex_out.view_ray;

	vec4 mat_data = textureLod(mat_data_sampler, vertex_out.tex_coords, startLod);
	vec3 N = decode_normal(mat_data.rg);

	vec3 V = -normalize(P);

	vec3 dir = -reflect(V, N);

	vec2 raycast_hit_uv;
	vec3 raycast_hit_point;
	if(traceScreenSpaceRay1(P+dir, dir, pcs.projection, depth_sampler,
							textureSize, 1.0, global_uniforms.proj_planes.x,
							max(1, 1), 0.1, 128, 20.0, int(startLod),
							raycast_hit_uv, raycast_hit_point)) {

		float roughness = mat_data.b;

		vec3 L = raycast_hit_point - P;
		float L_length = length(L);
		L /= L_length;

		vec3 H = normalize(V+L);

		float lod = mix(0.0, pcs.arguments.y, clamp(roughness + mix(-0.2, 0.2, L_length*0.5), 0.0, 1.0));
		vec3 radiance = textureLod(color_sampler, raycast_hit_uv/textureSize, lod).rgb;

		out_color.rgb = radiance;
		out_color.a = mix(1.0, 0.0, min(1.0, L_length*L_length / 400.0));
	}
}

