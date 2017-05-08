#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "normal_encoding.glsl"

layout(location = 0) in Vertex_data {
	vec3 world_pos;
	vec2 tex_coords;
} vertex_out;

layout(location = 0) out vec4 shadowmap_out;

layout(set=1, binding = 0) uniform sampler2D albedo_sampler;

layout(binding = 0) uniform Global_uniforms {
	mat4 view_proj;
	mat4 inv_view_proj;
	vec4 eye_pos;
	vec4 proj_planes;
} global_uniforms;


void main() {
	vec4 albedo = texture(albedo_sampler, vertex_out.tex_coords);

	if(albedo.a < 0.1)
		discard;

	float z = gl_FragCoord.z;//length(vertex_out.world_pos - global_uniforms.eye_pos.xyz);
	float m1 = z;
	float m2 = m1*m1;

	// bias y based on deriviative
	float dx = dFdx(z);
	float dy = dFdy(z);
	m2 += 0.25*(dx*dx+dy*dy);

	shadowmap_out = vec4(m1, m2, 0, 0);

//	shadowmap_out = vec4(exp(120.0 * z), 0, 0, 1);
}
