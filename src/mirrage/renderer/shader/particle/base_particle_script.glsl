#include "../random.glsl"

struct Random_vec4 {
	vec4 mean_hsva;
	vec4 stddev_hsva;
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
	vec4 ttl;      // ttl_left, ttl_initial, <empty>, <empty>
};

layout(std140, set=0, binding = 0) uniform Shared_uniforms {
	// TODO ?
	int effector_count;

	int padding1;
	int padding2;
	int padding3;

	Effector effectors[];
} shared_uniforms;

//layout(set=1, binding = 0) uniform sampler2D color_sampler;

layout(std430, set=0, binding = 1) readonly buffer Particles_old {
	Particle particles[];
} pin;

layout(std430, set=0, binding = 2) writeonly buffer Particles_new {
	Particle particles[];
} pout;

struct Emitter_particle_range {
	int offset;
	int count;
};

layout(std430, set=0, binding = 3) buffer Feedback_buffer {
	Emitter_particle_range ranges[];
} feedback;

layout(std430, set=0, binding = 4) buffer Feedback_mapping {
	uint new_feedback_index[];
} feedback_mapping;


vec3 quaternion_rotate(vec3 dir, vec4 q) {
	return dir + 2.0 * cross(q.xyz, cross(q.xyz, dir) + q.w * dir);
}
