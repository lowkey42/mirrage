#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable


layout(location = 0) in Vertex_data {
	vec2 tex_coords;
} vertex_out;

layout(location = 0) out vec4 out_color;

layout(set=1, binding = 1) uniform sampler2D depth_sampler;
layout(set=1, binding = 2) uniform sampler2D mat_data_sampler;
layout(set=1, binding = 3) uniform sampler2D gi_sampler;
layout(set=1, binding = 4) uniform sampler2D albedo_sampler;

layout(push_constant) uniform Push_constants {
	mat4 reprojection;
	vec4 arguments;
} pcs;


void main() {
	const float PI = 3.14159265359;

	vec3 radiance = textureLod(gi_sampler, vertex_out.tex_coords, pcs.arguments.r).rgb;
	vec3 albedo = textureLod(albedo_sampler, vertex_out.tex_coords, 0.0).rgb;
	vec4 mat_data = textureLod(mat_data_sampler, vertex_out.tex_coords, 0.0);
	float roughness = mat_data.b;
	float metallic = mat_data.a;

	vec3 F0 = mix(vec3(0.04), albedo.rgb, metallic);
	albedo.rgb *= 1.0 - min(metallic, 0.9);

	vec3 color = albedo / PI * radiance;

	out_color = vec4(color*0.2, 0.0);
}
