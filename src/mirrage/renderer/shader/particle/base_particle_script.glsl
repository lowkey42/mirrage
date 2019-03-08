#include "data_structures.glsl"

layout(std140, set=0, binding = 0) uniform Shared_uniforms {
	int effector_count;
	int global_effector_count;

	int padding2;
	int padding3;

	Effector effectors[];
} shared_uniforms;

layout(std430, set=0, binding = 1) readonly buffer Particles_old {
	Particle particles[];
} pin;

layout(std430, set=0, binding = 2) writeonly buffer Particles_new {
	Particle particles[];
} pout;

layout(std430, set=0, binding = 3) buffer Feedback_buffer {
	Emitter_particle_range ranges[];
} feedback;

layout(std430, set=0, binding = 4) buffer Feedback_mapping {
	uint new_feedback_index[];
} feedback_mapping;

//layout(set=5, binding = 0) uniform sampler2D color_sampler;
