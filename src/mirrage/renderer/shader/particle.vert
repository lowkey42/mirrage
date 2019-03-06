#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "global_uniforms.glsl"
#include "particle/data_structures.glsl"


layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 tex_coords;

layout(location = 3) in vec4 particle_position;
layout(location = 4) in vec4 particle_velocity;
layout(location = 5) in vec4 particle_ttl;

layout(location = 0) out vec3 out_view_pos;
layout(location = 1) out vec3 out_normal;
layout(location = 2) out vec2 out_tex_coords;
layout(location = 3) out vec4 out_particle_velocity;
layout(location = 4) out vec4 out_particle_ttl;

layout(std140, set=2, binding = 0) uniform Particle_config {
	Random_vec4 color; // hsva
	Random_vec4 color_change;

	Random_vec4 size;
	Random_vec4 size_change;

	Random_float sprite_rotation;
	Random_float sprite_rotation_change;

	float base_mass;
	float density;
	float drag;

	float timestep;
	uint particle_offset;
	uint particle_count;
	int padding;

	int effector_count;
	Effector effectors[];
} particle_config;

layout(push_constant) uniform Per_model_uniforms {
	mat4 model_to_view;
	vec4 light_color;
	vec4 options;
} model_uniforms;

out gl_PerVertex {
	vec4 gl_Position;
};

void main() {
	uint seed = floatBitsToUint(particle_velocity.w);
	float age = particle_ttl.y - particle_ttl.x;

	vec3 p = position;
	p *= rand_vec4(particle_config.size, seed,20).xyz + age*rand_vec4(particle_config.size_change, seed,24).xyz;
	// TODO: rotation

	vec4 view_pos = model_uniforms.model_to_view * vec4(p + particle_position.xyz, 1.0);

	out_view_pos = view_pos.xyz / view_pos.w;
	out_normal  = (model_uniforms.model_to_view *  vec4(normal, 0.0)).xyz;
	out_tex_coords = tex_coords;
	out_particle_velocity = particle_velocity;
	out_particle_ttl = particle_ttl;

	gl_Position = global_uniforms.proj_mat * view_pos;
}
