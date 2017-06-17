#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable


layout(location = 0) in Vertex_data {
	vec2 tex_coords;
	vec3 view_ray;
	flat vec3 corner_view_rays[4];
} vertex_out;

layout(location = 0) out vec4 out_color;

layout (constant_id = 0) const int KERNEL_SIZE = 64;
layout (constant_id = 1) const float RADIUS = 2.0;
layout (constant_id = 2) const float BIAS = 0.01;

layout(set=1, binding = 0) uniform sampler2D depth_sampler;
layout(set=1, binding = 1) uniform sampler2D mat_data_sampler;

#include "global_uniforms.glsl"
#include "normal_encoding.glsl"
#include "poisson.glsl"
#include "random.glsl"


const vec3 samples[64] = vec3[](
        vec3(-0.0602466, -0.072542, 0.0290215),
        vec3(-0.0397808, -0.0504498, 0.0055491),
        vec3(0.0339263, -0.0165469, 0.0170678),
        vec3(0.0795359, 0.0114222, 0.0218773),
        vec3(-0.0149484, 0.0188872, 0.0432702),
        vec3(-0.0189797, 0.0603666, 0.000940415),
        vec3(0.0610844, 0.0512505, 0.0540597),
        vec3(0.0127837, 0.06717, 0.0119518),
        vec3(0.000335198, 0.0111872, 0.0398142),
        vec3(-0.0280684, 0.0193634, 0.0192688),
        vec3(-0.0701847, -0.0615425, 0.0574746),
        vec3(0.0381073, -0.0116404, 0.0119745),
        vec3(-0.0254141, -0.0623273, 0.0115668),
        vec3(0.00182989, -0.00575836, 0.0120724),
        vec3(-0.000102374, -0.0127446, 0.00653706),
        vec3(0.0270965, -0.0334378, 0.0536093),
        vec3(0.0481464, -0.0381927, 0.0446291),
        vec3(-0.00667514, 0.0112687, 0.0201321),
        vec3(-0.000191688, -0.0264452, 0.0299985),
        vec3(0.0211234, 0.00478001, 0.0105501),
        vec3(0.0908851, -0.0504953, 0.00155828),
        vec3(-0.128757, -0.0364809, 0.0666071),
        vec3(0.0107151, -0.00163477, 0.0117708),
        vec3(0.0264095, -0.0280685, 0.0396517),
        vec3(-0.131058, -0.0827981, 0.0436305),
        vec3(-0.0411907, 0.0194653, 0.0168939),
        vec3(-0.150598, 0.107968, 0.147244),
        vec3(-0.141537, -0.154248, 0.0865182),
        vec3(-0.0708905, -0.050541, 0.0830867),
        vec3(0.0529127, -0.106897, 0.07148),
        vec3(-0.00157632, -0.0610906, 0.0034922),
        vec3(-0.0404827, 0.044029, 0.0398179),
        vec3(-0.19149, -0.0366789, 0.194243),
        vec3(0.119609, -0.052742, 0.0358214),
        vec3(-0.0181982, 0.032712, 0.0622328),
        vec3(0.0468141, 0.00759324, 0.00786538),
        vec3(-0.332879, -0.110327, 0.0418961),
        vec3(-0.257814, -0.13092, 0.0967134),
        vec3(-0.0844642, 0.150203, 0.219542),
        vec3(-0.220972, -0.0642296, 0.223154),
        vec3(-0.180754, 0.215907, 0.203917),
        vec3(0.139502, 0.137657, 0.190025),
        vec3(0.210093, 0.12219, 0.0043223),
        vec3(-0.00268955, 0.0493365, 0.188259),
        vec3(0.133209, -0.0487622, 0.119778),
        vec3(-0.103132, -0.160681, 0.079077),
        vec3(0.0189927, -0.0238069, 0.0681754),
        vec3(0.215426, -0.050772, 0.197216),
        vec3(-0.267465, -0.237106, 0.154746),
        vec3(-0.311742, -0.021473, 0.337472),
        vec3(0.062245, 0.248421, 0.41024),
        vec3(0.0288177, 0.101311, 0.0516795),
        vec3(-0.0312757, -0.443513, 0.370587),
        vec3(0.206546, -0.373067, 0.0871253),
        vec3(0.284678, 0.122253, 0.265739),
        vec3(0.277398, 0.290678, 0.0179605),
        vec3(-0.0284532, -0.258396, 0.316766),
        vec3(-0.183006, -0.0187287, 0.712289),
        vec3(-0.195132, -0.231197, 0.0683318),
        vec3(-0.312, 0.0901602, 0.257809),
        vec3(-0.0265297, 0.0821202, 0.0180493),
        vec3(0.607196, -0.306592, 0.24317),
        vec3(-0.0270172, -0.121396, 0.139532),
        vec3(0.144283, 0.215957, 0.154323)
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
		float range_check = 1.0 - smoothstep(0.0, 1.0, RADIUS / abs(P.z - actual.z));
		range_check = 1.0 - range_check*range_check;
		range_check *= clamp(abs(min(0.0, actual.z)), 0.0, 1.0);
		occlusion += (actual.z > expected.z + BIAS ? 1.0 : 0.0) * range_check;
	}
	out_color.r = 1.0 - (occlusion / KERNEL_SIZE);
	out_color.r = out_color.r * out_color.r;
}
