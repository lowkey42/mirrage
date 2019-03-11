#include "../random.glsl"

vec3 quaternion_rotate(vec3 v, vec4 q) {
	return v + 2.0 * cross(cross(v, q.xyz ) + q.w*v, q.xyz);
}

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
	return range.mean + range.stddev*rand;
}
vec3 rand_xyz(Random_vec4 range, vec3 rand) {
	return range.mean.xyz + range.stddev.xyz*rand;
}
vec4 rand_quat(Random_vec4 range, vec3 rand) {
	range.mean   *= vec4(0.5, 1, 1, 1) * 3.14159265359;
	range.stddev *= vec4(0.5, 1, 1, 1) * 3.14159265359;

	vec2 angles = range.mean.xy + range.stddev.xy * rand.xy;
	vec3 axis = vec3(cos(angles.x)*cos(angles.y),
	                 cos(angles.x)*sin(angles.y),
	                 sin(angles.x)).xzy;

	float angle = range.mean.z + range.stddev.z * rand.z;
	float half_angle = angle/2.0;
	float sha = sin(half_angle);

	return vec4(axis * sha, cos(half_angle));
}
vec3 rand_dir(vec2 mean_angles, vec2 stddev_angles, vec2 rand) {
	vec2 rot = stddev_angles * rand*3.14159265359;
	vec3 rand_dir = vec3(cos(rot.x)*cos(rot.y),
	                     cos(rot.x)*sin(rot.y),
						 sin(rot.x)).xzy;

	vec3 base_dir = vec3(cos(mean_angles.x)*cos(mean_angles.y),
	                     cos(mean_angles.x)*sin(mean_angles.y),
						 sin(mean_angles.x)).xzy;

	if(base_dir.x <= -1.0) {
		rand_dir = vec3(-1, -1, 1) * rand_dir;

	} else if(base_dir.x < 1.0) {
		vec3 mx = normalize(base_dir);
		vec3 my = normalize(cross(mx, vec3(1,0,0)));
		vec3 mz = normalize(cross(mx, my));
		rand_dir = mat3(mx,my,mz) * rand_dir;
	}

	return rand_dir;
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
	vec4 position; // xyz + ttl_left
	vec4 velocity; // xyz + ttl_initial
	uvec4 data;    // last_feedback_buffer_index, seed, keyframe, floatBitsToUint(keyframe_interpolation_factor)
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
	Random_vec4 rotation; // elevation, azimuth, angle
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


