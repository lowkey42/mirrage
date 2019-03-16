#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "particle/data_structures.glsl"

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 tex_coords;

layout(location = 3) in vec4 particle_position;
layout(location = 4) in vec4 particle_velocity;
layout(location = 5) in uvec4 particle_data;

layout(location = 0) out vec4 out_view_pos;
layout(location = 1) out vec3 out_normal;
layout(location = 2) out vec2 out_tex_coords;
layout(location = 3) out vec4 out_particle_color;
layout(location = 4) out vec4 out_screen_pos;

layout(std140, set=4, binding = 0) readonly buffer Particle_type_config {
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

#include "particle.vert_base.glsl"

void main() {
	base_main();
}
