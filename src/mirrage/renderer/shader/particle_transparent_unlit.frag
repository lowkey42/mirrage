#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "particle_transparent_base.glsl"

vec4 calc_color(int mip) {
	vec4 albedo = texture(albedo_sampler, tex_coords);
	albedo *= out_particle_color;

	vec2 screen_uv = out_screen_pos.xy/out_screen_pos.w*0.5+0.5;
	float background = subpassLoad(depth_sampler).r;
	albedo.a *= 1.0 - max(0.0, view_pos.z-to_linear_depth(background))/0.1;

	if(albedo.a<0.00001) {
		discard;
	}

	return vec4(albedo.rgb, 1.0) * albedo.a;
}
