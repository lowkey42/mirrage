#include "global_uniforms.glsl"
#include "normal_encoding.glsl"
#include "brdf.glsl"
#include "particle/data_structures.glsl"

layout(early_fragment_tests) in;

layout(location = 0) in vec4 view_pos;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 tex_coords;
layout(location = 3) in vec4 out_particle_color;
layout(location = 4) in vec4 out_screen_pos;

layout(location = 0) out vec4 accum_out;
layout(location = 1) out vec4 revealage_out;

layout(set=3, binding = 0) uniform sampler2D albedo_sampler;
layout(set=3, binding = 1) uniform sampler2D normal_sampler;
layout(set=3, binding = 2) uniform sampler2D brdf_sampler;
layout(set=3, binding = 3) uniform sampler2D emission_sampler;

layout(std140, set=4, binding = 0) readonly buffer Particle_type_config {
	PARTICLE_TYPE_CONFIG
} particle_config;

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

layout(set=1, binding = 1) uniform sampler2D depth_sampler;

layout(set=2, binding = 0) uniform texture2D shadowmaps[1];
layout(set=2, binding = 1) uniform samplerShadow shadowmap_shadow_sampler; // sampler2DShadow
layout(set=2, binding = 2) uniform sampler shadowmap_depth_sampler; // sampler2D

layout(push_constant) uniform Per_model_uniforms {
	mat4 model;
	vec4 light_color;
	vec4 light_data;
	vec4 light_data2; // R=shadowmapID
	vec4 shadow_color;
} model_uniforms;

vec4 calc_color(int mip);

void main() {
	int mip = int(model_uniforms.light_data2.w);
	vec4 result_color = calc_color(mip);

	/* Modulate the net coverage for composition by the transmission. This does not affect the color channels of the
	  transparent surface because the caller's BSDF model should have already taken into account if transmission modulates
	  reflection. This model doesn't handled colored transmission, so it averages the color channels. See

	  McGuire and Enderton, Colored Stochastic Shadow Maps, ACM I3D, February 2011
	  http://graphics.cs.williams.edu/papers/CSSM/

	  for a full explanation and derivation.*/

	//result_color.a *= 1.0 - clamp((transmit.r + transmit.g + transmit.b) * (1.0 / 3.0), 0, 1);

	/* You may need to adjust the w function if you have a very large or very small view volume; see the paper and
	   presentation slides at http://jcgt.org/published/0002/02/09/ */
	// Intermediate terms to be cubed
	float a = min(1.0, result_color.a) * 8.0 + 0.01;
	float b = -gl_FragCoord.z * 0.95 + 1.0;

	/* If your scene has a lot of content very close to the far plane,
	   then include this line (one rsqrt instruction):
	   b /= sqrt(1e4 * abs(csZ)); */
	float w       = clamp(a * a * a * 1e8 * b * b * b, 1e-2, 3e2);
	accum_out     = result_color * w;
	revealage_out = vec4(result_color.a, 0,0,0);
}
