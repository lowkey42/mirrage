#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "particle_transparent_base.glsl"

vec4 calc_color(int mip) {
	vec4 albedo = texture(albedo_sampler, tex_coords);
	albedo *= out_particle_color;

	vec2 screen_uv = out_screen_pos.xy/out_screen_pos.w*0.5+0.5;
	float background_depth = textureLod(depth_sampler, screen_uv, mip).r * -global_uniforms.proj_planes.y;
	albedo.a *= smoothstep(0, 0.5, abs(background_depth-view_pos.z));

	if(albedo.a<0.001) {
		discard;
	}

	return vec4(albedo.rgb, 1.0) * albedo.a;
}
