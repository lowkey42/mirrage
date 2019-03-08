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

vec2 normal_rand(uint seed, uint i, uint j) {
	vec2 uv = vec2(floatConstruct(hash(uvec2(seed, i))), floatConstruct(hash(uvec2(seed, j))));
	float x = 6.28318530718 * uv[1];
	return sqrt(-2.0*log(uv[0])) * vec2(cos(x),sin(x));
}
float rand_float(Random_float range, float rand) {
	return range.mean + range.stddev*rand;
}
vec4 rand_vec4(Random_vec4 range, uint seed, uint range_begin) {
	vec4 rand = vec4(normal_rand(seed, range_begin, range_begin+1), normal_rand(seed, range_begin+2, range_begin+4));
	return range.mean + range.stddev*rand;
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
	// seed: 0=ttl, 1=velocity, 2+3=direction, TODO: position?
	//       10-13=color, 14-17=color_change
	//       20-23=size,  24-27=size_change
	//       30=rotation, 31=rotation_change
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

struct Particle_type_config {
	uint normal_distribution_flags;
	// 1 <<  0	:	color[0] normal/uniform
	// 1 <<  1	:	color[1] normal/uniform
	// 1 <<  2	:	color[2] normal/uniform
	// 1 <<  3	:	color[3] normal/uniform
	// 1 <<  4	:	rotation[0] normal/uniform
	// 1 <<  5	:	rotation[1] normal/uniform
	// 1 <<  6	:	rotation[2] normal/uniform
	// 1 <<  7	:	size[0] normal/uniform
	// 1 <<  8	:	size[1] normal/uniform
	// 1 <<  9	:	size[2] normal/uniform

	uint rotate_with_velocity; // bool
	uint keyframe_count;

	uint padding;

	Particle_keyframe keyframes;
};

vec3 quaternion_rotate(vec3 dir, vec4 q) {
	return dir + 2.0 * cross(q.xyz, cross(q.xyz, dir) + q.w * dir);
}
