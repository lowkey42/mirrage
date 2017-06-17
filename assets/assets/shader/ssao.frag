#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable


layout(location = 0) in Vertex_data {
	vec2 tex_coords;
	vec3 view_ray;
	flat vec3 corner_view_rays[4];
} vertex_out;

layout(location = 0) out vec4 out_color;

layout (constant_id = 0) const int KERNEL_SIZE = 16;
layout (constant_id = 1) const float RADIUS = 1.0;
layout (constant_id = 2) const float BIAS = 0.05;

layout(set=1, binding = 0) uniform sampler2D depth_sampler;
layout(set=1, binding = 1) uniform sampler2D mat_data_sampler;

#include "global_uniforms.glsl"
#include "normal_encoding.glsl"
#include "poisson.glsl"
#include "random.glsl"


const vec3 samples[32] = vec3[](
		vec3(-0.0622651, -0.0516783, 0.0374449),
		vec3(0.0302176, -0.0200379, 0.0166269),
		vec3(-0.0390986, 0.0401571, 0.00738356),
		vec3(0.00983945, 0.0370422, 0.035414),
		vec3(0.0523208, 0.0687755, 0.0504415),
		vec3(-0.0280151, -0.0102105, 0.0620991),
		vec3(0.0602267, 0.069226, 0.0123528),
		vec3(0.0285927, -0.0144757, 0.0328993),
		vec3(-0.0625902, 0.00486652, 0.0646069),
		vec3(0.00588934, 0.00137853, 0.00922296),
		vec3(0.0345845, -0.0213982, 0.0946942),
		vec3(-0.000672068, 0.0240709, 0.00316105),
		vec3(-0.0166647, 0.0169044, 0.00253135),
		vec3(0.0127063, 0.00861719, 0.0402493),
		vec3(-0.0185948, -0.0267613, 0.138175),
		vec3(0.00785665, -0.0342657, 0.0356098),
		vec3(-0.00279815, -0.0268175, 0.00407524),
		vec3(-0.00839346, -0.0667035, 0.0363405),
		vec3(0.00458419, 0.0101013, 0.00270968),
		vec3(0.0271658, -0.00899945, 0.00779216),
		vec3(0.11128, 0.119109, 0.0137799),
		vec3(0.0379422, 0.053585, 0.0952886),
		vec3(-0.0731075, -0.0513046, 0.0836565),
		vec3(0.0495465, -0.0285149, 0.0123632),
		vec3(-0.0281168, -0.10963, 0.0879173),
		vec3(-0.164272, -0.152799, 0.0194657),
		vec3(0.0138705, -0.156125, 0.0938625),
		vec3(-0.0235614, -0.0525998, 0.00553039),
		vec3(-0.157122, 0.0382268, 0.0307511),
		vec3(-0.154707, 0.0928639, 0.0572261),
		vec3(-0.173392, -0.123376, 0.0691542),
		vec3(-0.0563887, 0.17184, 0.149005)
);

vec3 to_view_space(vec2 uv) {
	float depth = texelFetch(depth_sampler, ivec2(uv * textureSize(depth_sampler, 0)), 0).r;

	vec3 view_ray_x1 = mix(vertex_out.corner_view_rays[0], vertex_out.corner_view_rays[1], uv.x);
	vec3 view_ray_x2 = mix(vertex_out.corner_view_rays[2], vertex_out.corner_view_rays[3], uv.x);

	return mix(view_ray_x1, view_ray_x2, uv.y) * depth;
}

vec2 to_uv(vec3 pos) {
	vec4 p = global_uniforms.proj_mat * vec4(pos, 1.0);
	return (p.xy / p.w) * 0.5 + 0.5;
}

// based on https://learnopengl.com/#!Advanced-Lighting/SSAO
void main() {
	ivec2 center_px = ivec2(vertex_out.tex_coords*textureSize(depth_sampler, 0));

	float depth = texelFetch(depth_sampler, center_px, 0).r;
	vec3 P = depth * vertex_out.view_ray;

	vec3 N = decode_normal(texelFetch(mat_data_sampler, center_px, 0).rg);


	vec3 random = normalize(vec3(PDnrand3(vertex_out.tex_coords).xy*2-1, 0.0));
	vec3 tangent = normalize(random - N * dot(random, N));
	vec3 bitangent = cross(N, tangent);
	mat3 TBN = mat3(tangent, bitangent, N);

	// iterate over the sample kernel and calculate occlusion factor
	float occlusion = 0.0;
	for(int i = 0; i < KERNEL_SIZE; ++i) {
		// expected view space position of sample
		vec3 expected = P + TBN * samples[i] * RADIUS;

		// actual view space position of sample (reconstructed from gBuffer depth)
		vec3 actual = to_view_space(to_uv(expected));

		// range check & accumulate
		float rangeCheck = smoothstep(0.0, 1.0, RADIUS / abs(P.z - actual.z));
		rangeCheck *= clamp(abs(min(0.0, actual.z)), 0.0, 1.0);
		occlusion += (actual.z >= expected.z + BIAS ? 1.0 : 0.0) * rangeCheck;
	}
	out_color.r = smoothstep(0.2, 0.95, 1.0 - (occlusion / KERNEL_SIZE));
	out_color.r = out_color.r * out_color.r;
}
