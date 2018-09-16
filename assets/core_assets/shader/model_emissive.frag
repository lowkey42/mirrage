#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "global_uniforms.glsl"
#include "normal_encoding.glsl"

layout(location = 0) in vec3 view_pos;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 tex_coords;


layout(location = 0) out vec4 depth_out;
layout(location = 1) out vec4 albedo_mat_id;
layout(location = 2) out vec4 mat_data;
layout(location = 3) out vec4 color_out;
layout(location = 4) out vec4 color_diffuse_out;

layout(set=1, binding = 0) uniform sampler2D albedo_sampler;
layout(set=1, binding = 1) uniform sampler2D mat_data_sampler;
layout(set=1, binding = 2) uniform sampler2D mat_data2_sampler;

layout(push_constant) uniform Per_model_uniforms {
	mat4 model;
	vec4 light_color;
	vec4 material_properties;
} model_uniforms;


void main() {
	vec4 albedo = texture(albedo_sampler, tex_coords);

	if(albedo.a < 0.1)
		discard;

	depth_out     = vec4(-view_pos.z / global_uniforms.proj_planes.y, 0,0,1);
	albedo_mat_id = vec4(albedo.rgb, 1.0);
	mat_data      = vec4(encode_normal(normalize(normal)), 0.0, 0.0);
	color_out     = vec4(albedo.rgb * texture(mat_data2_sampler, tex_coords).r * model_uniforms.material_properties.rgb, 1.0);
	color_diffuse_out = color_out;
}
