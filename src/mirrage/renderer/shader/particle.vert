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
layout(location = 5) in uvec4 particle_data;

layout(location = 0) out vec3 out_view_pos;
layout(location = 1) out vec3 out_normal;
layout(location = 2) out vec2 out_tex_coords;
layout(location = 3) out vec4 out_particle_color;

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
vec4 calc_rotation(uint keyframe_a, uint keyframe_b, float t, vec3 rand) {
	return normalize(mix(rand_quat(particle_config.keyframes[keyframe_a].rotation, rand),
	                     rand_quat(particle_config.keyframes[keyframe_b].rotation, rand),
	                     t));
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
	size.y = (particle_config.normal_distribution_flags & 32)!=0 ? normal_size.x
	                                                             : uniform_size.x*2-1;
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
	uint seed = particle_data.y;

	vec3 rand_rotation;
	vec3 rand_size;
	vec4 rand_color;
	calc_random(seed, rand_rotation, rand_size, rand_color);


	uint keyframe_a = particle_data.z;
	uint keyframe_b = min(keyframe_a+1, particle_config.keyframe_count-1);
	float keyframe_t = uintBitsToFloat(particle_data.w);

	vec3 size = max(vec3(0,0,0), calc_size(keyframe_a, keyframe_b, keyframe_t, rand_size));
	if((particle_config.flags&4)!=0)
		size.y = size.z = size.x;

	vec4 p = vec4(position * size, 1.0);
	vec4 n = vec4(normal, 0.0);

	vec4 rotation = calc_rotation(keyframe_a, keyframe_b, keyframe_t, rand_rotation);
	p.xyz = quaternion_rotate(p.xyz, rotation);
	n.xyz = quaternion_rotate(n.xyz, rotation);

	uint rotate_with_velocity = particle_config.flags & 3;

	if(rotate_with_velocity==2) {
		vec4 view_vel = (global_uniforms.view_mat * vec4(particle_velocity.xyz, 0.0));

		float len_2d = length(view_vel.xy);

		if(len_2d >= 0.001) {
			view_vel.xy = view_vel.xy / len_2d;
			float angle = (view_vel.y<0? -1.0 : 1.0) * acos(view_vel.x);
			angle -= 3.1415926*0.5;

			float sa = sin(angle);
			float ca = cos(angle);
			p.xy = vec2(p.x*ca - p.y*sa, p.x*sa + p.y*ca);
			n.xy = vec2(n.x*ca - n.y*sa, n.x*sa + n.y*ca);
		}

	} else if(rotate_with_velocity==1) {
		vec3 dir      = particle_velocity.xyz;
		float dir_len = length(dir);
		if(dir_len > 0) {
			dir /= dir_len;
			if(dir.y <= -1.0) {
				p.xyz = vec3(-p.x, -p.y, p.z);
				n.xyz = vec3(-n.x, -n.y, n.z);
			} else if(dir.y < 1.0) {
				vec3 my = normalize(dir);
				vec3 mz = normalize(cross(my, vec3(0,1,0)));
				vec3 mx = normalize(cross(my, mz));
				p.xyz = mat3(mx,my,mz) * p.xyz;
				n.xyz = mat3(mx,my,mz) * n.xyz;
			}
		}
	}


	p = model_uniforms.model_to_view * p;
	n = model_uniforms.model_to_view * n;

	vec4 view_pos = global_uniforms.view_mat * vec4(p.xyz + particle_position.xyz, 1.0);

	out_view_pos = view_pos.xyz / view_pos.w;
	out_normal  = (global_uniforms.view_mat * n).xyz;
	out_tex_coords = tex_coords;
	vec4 clip_rect = particle_config.keyframes[keyframe_t<0.5 ? keyframe_a : keyframe_b].clip_rect;
	out_tex_coords.y = 1-out_tex_coords.y;
	out_tex_coords = clip_rect.xy + out_tex_coords * clip_rect.zw;
	out_tex_coords.y = 1-out_tex_coords.y;

	vec4 color = calc_color(keyframe_a, keyframe_b, keyframe_t, rand_color);;
	out_particle_color = vec4(hsv2rgb(color.xyz), color.a);

	gl_Position = global_uniforms.proj_mat * view_pos;
}
