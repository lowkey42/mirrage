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
layout(location = 5) out vec4 out_shadows;

struct Directional_light {
	mat4 light_space;
	vec4 radiance;
	vec4 shadow_radiance;
	vec4 dir;	// + int shadowmap;
};

layout(std430, set=1, binding = 0) readonly buffer Light_data {
	int count;
	int padding[3];

	Directional_light dir_lights[];
} lights;

layout(set=2, binding = 0) uniform texture2D shadowmaps[1];
layout(set=2, binding = 1) uniform samplerShadow shadowmap_shadow_sampler; // sampler2DShadow
layout(set=2, binding = 2) uniform sampler shadowmap_depth_sampler; // sampler2D

layout(std140, set=4, binding = 0) readonly buffer Particle_type_config {
	PARTICLE_TYPE_CONFIG
} particle_config;

layout (constant_id = 0) const int FRAGMENT_SHADOWS = 1;

layout(push_constant) uniform Per_model_uniforms {
	mat4 model_to_view;
	vec4 light_color;
	vec4 options;
} model_uniforms;

out gl_PerVertex {
	vec4 gl_Position;
};

#include "particle.vert_base.glsl"

float sample_shadow(mat4 light_space, int shadowmap, vec3 p) {
	vec4 lightspace_pos = light_space * vec4(p, 1);
	lightspace_pos /= lightspace_pos.w;
	lightspace_pos.xy = lightspace_pos.xy * 0.5 + 0.5;

	if(lightspace_pos.x<0 || lightspace_pos.y<0 || lightspace_pos.x>=1 || lightspace_pos.y>=1)
		return 1.0;
	else {
		return texture(sampler2DShadow(shadowmaps[shadowmap], shadowmap_shadow_sampler),
		               lightspace_pos.xyz);
	}
}

void main() {
	base_main();

	if(FRAGMENT_SHADOWS==0) {
		vec3 p = out_view_pos.xyz / out_view_pos.w;

		for(int i=0; i<lights.count; i++) {
			int shadowmap = int(lights.dir_lights[i].dir.w);
			if(shadowmap<0) {
				out_shadows[shadowmap] = 1.0;
			} else {
				mat4 ls = lights.dir_lights[i].light_space;
				out_shadows[shadowmap] = (sample_shadow(ls, shadowmap, p-vec3(0,0,0.05))
				                          + sample_shadow(ls, shadowmap, p)
				                          + sample_shadow(ls, shadowmap, p+vec3(0,0,0.05))) / 3.0;
			}
		}
	} else {
		out_shadows = vec4(1);
	}
}
