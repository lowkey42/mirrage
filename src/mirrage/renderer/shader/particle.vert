#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "global_uniforms.glsl"
#include "particle/data_structures.glsl"
#include "color_conversion.glsl"


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
layout(location = 5) out vec4 out_particle_color;

layout(std140, set=2, binding = 0) readonly buffer Particle_type_config {
	PARTICLE_TYPE_CONFIG
} particle_config;

layout(push_constant) uniform Per_model_uniforms {
	mat4 model_to_view;
	vec4 light_color;
	vec4 options;
} model_uniforms;

out gl_PerVertex {
	vec4 gl_Position;
};

vec3 calc_size(uint keyframe_a, uint keyframe_b, float t, vec3 rand) {
	return mix(rand_xyz(particle_config.keyframes[keyframe_a].size, rand),
	           rand_xyz(particle_config.keyframes[keyframe_b].size, rand),
	           t);
}
vec4 calc_color(uint keyframe_a, uint keyframe_b, float t, vec4 rand) {
	return mix(rand_vec4(particle_config.keyframes[keyframe_a].color, rand),
	           rand_vec4(particle_config.keyframes[keyframe_b].color, rand),
	           t);
}

void calc_random(uint seed, out vec3 rotation, out vec3 size, out vec4 color) {
	vec2 uniform_rotation = uniform_rand(seed, 20, 21);
	vec2 uniform_shared   = uniform_rand(seed, 22, 23);
	vec2 uniform_size     = uniform_rand(seed, 24, 25);
	vec4 uniform_color    = vec4(uniform_rand(seed, 10, 11), uniform_rand(seed, 12, 13));

	vec2 normal_rotation = uniform_to_normal_rand(uniform_rotation);
	vec2 normal_shared   = uniform_to_normal_rand(uniform_shared);
	vec2 normal_size     = uniform_to_normal_rand(uniform_size);
	vec4 normal_color    = vec4(uniform_to_normal_rand(uniform_color.xy), uniform_to_normal_rand(uniform_color.zw));


	color.x = (particle_config.normal_distribution_flags & 1)!=0 ? normal_color.x
	                                                             : uniform_color.x*2-1;
	color.y = (particle_config.normal_distribution_flags & 2)!=0 ? normal_color.y
	                                                             : uniform_color.y*2-1;
	color.z = (particle_config.normal_distribution_flags & 4)!=0 ? normal_color.z
	                                                             : uniform_color.z*2-1;
	color.w = (particle_config.normal_distribution_flags & 8)!=0 ? normal_color.w
	                                                             : uniform_color.w*2-1;

	size.x = (particle_config.normal_distribution_flags & 16)!=0 ? normal_shared.y
	                                                             : uniform_shared.y*2-1;
	size.y = (particle_config.normal_distribution_flags & 32)!=0 ? normal_rotation.x
	                                                             : uniform_rotation.x*2-1;
	size.z = (particle_config.normal_distribution_flags & 64)!=0 ? normal_size.x
	                                                             : uniform_size.x*2-1;

	rotation.x = (particle_config.normal_distribution_flags & 128)!=0 ? normal_rotation.x
	                                                                  : uniform_rotation.x*2-1;
	rotation.y = (particle_config.normal_distribution_flags & 256)!=0 ? normal_rotation.y
	                                                                  : uniform_rotation.y*2-1;
	rotation.z = (particle_config.normal_distribution_flags & 512)!=0 ? normal_shared.x
	                                                                  : uniform_shared.x*2-1;
}

void main() {
	uint seed = floatBitsToUint(particle_velocity.w);

	vec3 rand_rotation;
	vec3 rand_size;
	vec4 rand_color;
	calc_random(seed, rand_rotation, rand_size, rand_color);


	uint keyframe_a = floatBitsToUint(particle_ttl[2]);
	uint keyframe_b = min(keyframe_a+1, particle_config.keyframe_count-1);
	float keyframe_t = particle_ttl[3];

	vec3 size = max(vec3(0,0,0), calc_size(keyframe_a, keyframe_b, keyframe_t, rand_size));
	if(particle_config.symmetric_scaling!=0)
		size.y = size.z = size.x;

	vec3 p = position * size;
	// TODO: rotation

	vec4 view_pos = model_uniforms.model_to_view * vec4(p + particle_position.xyz, 1.0);

	out_view_pos = view_pos.xyz / view_pos.w;
	out_normal  = (model_uniforms.model_to_view *  vec4(normal, 0.0)).xyz;
	out_tex_coords = tex_coords;
	out_particle_velocity = particle_velocity;
	out_particle_ttl = particle_ttl;

	vec4 color = calc_color(keyframe_a, keyframe_b, keyframe_t, rand_color);;
	out_particle_color = vec4(hsv2rgb(color.xyz), color.a);

	gl_Position = global_uniforms.proj_mat * view_pos;
}
