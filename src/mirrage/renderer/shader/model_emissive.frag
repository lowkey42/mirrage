#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "global_uniforms.glsl"
#include "normal_encoding.glsl"

layout(early_fragment_tests) in;

layout(location = 0) in vec3 view_pos;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 tex_coords;


layout(location = 0) out vec4 color_out;
layout(location = 1) out vec4 color_diffuse_out;

layout(set=1, binding = 0) uniform sampler2D albedo_sampler;
layout(set=1, binding = 1) uniform sampler2D normal_sampler;
layout(set=1, binding = 2) uniform sampler2D brdf_sampler;
layout(set=1, binding = 3) uniform sampler2D emission_sampler;

layout(std140, set=1, binding = 4) uniform Material_uniforms {
    vec4 albedo;
    vec4 emission;
    float roughness;
    float metallic;
    float refraction;
    float has_normals;
} material;

layout(push_constant) uniform Per_model_uniforms {
	mat4 model;
	vec4 light_color;
	vec4 material_properties;
} model_uniforms;

const float PI = 3.14159265359;


vec3 decode_tangent_normal(vec2 tn);
vec3 tangent_space_to_world(vec3 N);

void main() {
	vec4 albedo = texture(albedo_sampler, tex_coords) * material.albedo;

	float emissive_power = texture(emission_sampler, tex_coords).r;
	albedo.rgb *= material.emission.rgb * material.emission.a;

	color_out     = vec4(albedo.rgb *  model_uniforms.material_properties.rgb * emissive_power * model_uniforms.material_properties.a * albedo.a, 0.0);
	color_diffuse_out = color_out;
}
