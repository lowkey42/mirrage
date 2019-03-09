#include "../random.glsl"

struct Random_vec4 {
	vec4 mean;
	vec4 stddev;
};
struct Random_float {
	float mean;
	float stddev;
	// padded to vec4 if used as std140
};

vec2 uniform_to_normal_rand(vec2 rand) {
	float x = 6.28318530718 * rand[1];
	return sqrt(-2.0*log(rand[0])) * vec2(cos(x),sin(x));
}
// 0-1
float uniform_rand(uint seed, uint i) {
	return floatConstruct(hash(uvec2(seed, i)));
}
vec2 uniform_rand(uint seed, uint i, uint j) {
	return vec2(uniform_rand(seed, i), uniform_rand(seed, j));
}

vec2 normal_rand(uint seed, uint i, uint j) {
	return uniform_to_normal_rand(uniform_rand(seed, i, j));
}

float rand_float(Random_float range, float rand) {
	return range.mean + range.stddev*rand;
}
vec4 rand_vec4(Random_vec4 range, vec4 rand) {
	// vec4 rand = vec4(normal_rand(seed, range_begin, range_begin+1), normal_rand(seed, range_begin+2, range_begin+4));
	return range.mean + range.stddev*rand;
}
vec3 rand_xyz(Random_vec4 range, vec3 rand) {
	return range.mean.xyz + range.stddev.xyz*rand;
}
vec3 rand_dir(vec2 mean_angles, vec2 stddev_angles, vec2 rand) {
	vec2 angles = mean_angles + stddev_angles * rand;
	return vec3(sin(angles.x)*cos(angles.y),
	            sin(angles.x)*sin(angles.y),
	            cos(angles.x));
}


struct Effector {
	vec4 force_dir; // w=padding
	vec4 position;  // w=padding

	float force;
	float distance_decay;
	float mass_scale; // used as a=mix(F/m, F, mass_scale)
	float fixed_dir;
};

struct Particle {
	vec4 position; // xyz + uintBitsToFloat(last_feedback_buffer_index)
	vec4 velocity; // xyz + seed
	vec4 ttl;      // ttl_left, ttl_initial, keyframe, keyframe_interpolation_factor
	// seed: 0=ttl, 1=velocity, 2+3=direction, 4+5=vel_direction, 6...=position
	//       10-13=color
	//       20-22=rotation, 23-25=size
};

struct Emitter_particle_range {
	int offset;
	int count;
};

struct Particle_keyframe {
	Random_vec4 color; // hsv + alpha
	Random_vec4 rotation; // pitch, yaw, roll
	Random_vec4 size; // xyz

	float time;
	float base_mass;
	float density;
	float drag;
};


/*
  normal_distribution_flags:
	1 <<  0 =   1	:	color[0] normal/uniform
	1 <<  1 =   2	:	color[1] normal/uniform
	1 <<  2 =   4	:	color[2] normal/uniform
	1 <<  3 =   8	:	color[3] normal/uniform
	1 <<  4 =  16	:	rotation[0] normal/uniform
	1 <<  5 =  32	:	rotation[1] normal/uniform
	1 <<  6 =  64	:	rotation[2] normal/uniform
	1 <<  7 = 128	:	size[0] normal/uniform
	1 <<  8 = 256	:	size[1] normal/uniform
	1 <<  9 = 512	:	size[2] normal/uniform
*/
#define PARTICLE_TYPE_CONFIG \
	uint normal_distribution_flags;\
	uint rotate_with_velocity; \
	uint symmetric_scaling; \
	uint keyframe_count; \
	Particle_keyframe[] keyframes;

vec3 quaternion_rotate(vec3 dir, vec4 q) {
	return dir + 2.0 * cross(q.xyz, cross(q.xyz, dir) + q.w * dir);
}
