#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "normal_encoding.glsl"
#include "poisson.glsl"
#include "random.glsl"


layout(location = 0) in Vertex_data {
	vec2 tex_coords;
} vertex_out;

layout(location = 0) out vec4 out_depth;
layout(location = 1) out vec4 out_mat_data;

layout(set=1, binding = 0) uniform sampler2D depth_sampler;
layout(set=1, binding = 1) uniform sampler2D mat_data_sampler;

layout(push_constant) uniform Push_constants {
	vec4 arguments;
} pcs;

const ivec2 offsets[16] = ivec2[](
    ivec2( 0, 0),
    ivec2( 1, 0),
    ivec2(1, 1),
    ivec2(0, 1),

ivec2(-1, -1),
ivec2( 0, -1),
ivec2( 1, -1),
ivec2( 2, -1),

ivec2(-1, 0),
ivec2( 2, 0),
ivec2(-1, 1),
ivec2( 2, 1),

ivec2(-1,  2),
ivec2( 0,  2),
ivec2( 1,  2),
ivec2( 2,  2)
);

float g1(float x) {
	float b = 0;
	float c = 1;
	return exp(- (x-b)*(x-b) / (2*c*c));
}
float g2(float x) {
	float b = 0;
	float c = 0.1;
	return exp(- (x-b)*(x-b) / (2*c*c));
}
float g3(float x) {
	float b = 0;
	float c = 0.01f;
	return exp(- (x-b)*(x-b) / (2*c*c));
}

void main() {
	vec2 tex_size = textureSize(depth_sampler, 0);

	const vec2 uv_00 = vertex_out.tex_coords + vec2(-1,-1) / tex_size;
	const vec2 uv_10 = vertex_out.tex_coords + vec2( 1,-1) / tex_size;
	const vec2 uv_11 = vertex_out.tex_coords + vec2( 1, 1) / tex_size;
	const vec2 uv_01 = vertex_out.tex_coords + vec2(-1, 1) / tex_size;
	const ivec2[] center_offsets = ivec2[4](ivec2(0,0), ivec2(1,0), ivec2(1,1), ivec2(0,1));

	vec4 depth_00 = textureGather(depth_sampler, uv_00, 0);
	vec4 depth_10 = textureGather(depth_sampler, uv_10, 0);
	vec4 depth_11 = textureGather(depth_sampler, uv_11, 0);
	vec4 depth_01 = textureGather(depth_sampler, uv_01, 0);

	float avg_depth = (dot(vec4(1), depth_00) + dot(vec4(1), depth_10)
	                 + dot(vec4(1), depth_11) + dot(vec4(1), depth_01)) / 16.0;

	vec4 center_depths = vec4(depth_00.y, depth_10.x, depth_11.w, depth_01.z);

	vec4 score = vec4(g3(avg_depth - center_depths.x),
	                  g3(avg_depth - center_depths.y),
	                  g3(avg_depth - center_depths.z),
	                  g3(avg_depth - center_depths.w) );


	vec4 normal_x_00 = textureGather(mat_data_sampler, uv_00, 0);
	vec4 normal_x_10 = textureGather(mat_data_sampler, uv_10, 0);
	vec4 normal_x_11 = textureGather(mat_data_sampler, uv_11, 0);
	vec4 normal_x_01 = textureGather(mat_data_sampler, uv_01, 0);

	float avg_normal_x = (dot(vec4(1), normal_x_00) + dot(vec4(1), normal_x_10)
	                    + dot(vec4(1), normal_x_11) + dot(vec4(1), normal_x_01)) / 16.0;

	vec4 center_normals_x = vec4(normal_x_00.y, normal_x_10.x, normal_x_11.w, normal_x_01.z);

	score *= vec4(g2(avg_normal_x - center_normals_x.x),
	              g2(avg_normal_x - center_normals_x.y),
	              g2(avg_normal_x - center_normals_x.z),
	              g2(avg_normal_x - center_normals_x.w) );


	vec4 normal_y_00 = textureGather(mat_data_sampler, uv_00, 1);
	vec4 normal_y_10 = textureGather(mat_data_sampler, uv_10, 1);
	vec4 normal_y_11 = textureGather(mat_data_sampler, uv_11, 1);
	vec4 normal_y_01 = textureGather(mat_data_sampler, uv_01, 1);

	float avg_normal_y = (dot(vec4(1), normal_y_00) + dot(vec4(1), normal_y_10)
	                    + dot(vec4(1), normal_y_11) + dot(vec4(1), normal_y_01)) / 16.0;

	vec4 center_normals_y = vec4(normal_y_00.y, normal_y_10.x, normal_y_11.w, normal_y_01.z);

	score *= vec4(g2(avg_normal_y - center_normals_y.x),
	              g2(avg_normal_y - center_normals_y.y),
	              g2(avg_normal_y - center_normals_y.z),
	              g2(avg_normal_y - center_normals_y.w) );

	int max_index = 0;
	if(score.y > score.x)
		max_index = 1;
	if(score.z > score.y)
		max_index = 2;
	if(score.w > score.z)
		max_index = 3;


	out_depth    = texelFetch(depth_sampler,    ivec2(vertex_out.tex_coords * tex_size) + center_offsets[max_index], 0);
	out_mat_data = texelFetch(mat_data_sampler, ivec2(vertex_out.tex_coords * tex_size) + center_offsets[max_index], 0);

/*

	float depth[4];
	vec4  mat_data[4];
	vec3  normal[4];
	float score[4];

	ivec2 center = ivec2(vertex_out.tex_coords*textureSize(depth_sampler, 0));

	for(int i=0; i<4; i++) {
		ivec2 p = center + offsets[i];

		depth[i]    = texelFetch(depth_sampler,    p, 0).r;
		mat_data[i] = texelFetch(mat_data_sampler, p, 0);
		normal[i]   = decode_normal(mat_data[i].rg);
		score[i] = 0;
	}
	
	// calc score
	for(int j=0; j<4; j++) {
		for(int i=0; i<j; i++) {
			score[i] += g1(vec2(offsets[j]-offsets[i]).length()) * g2(1.0 - dot(normal[j], normal[i])) * g3(depth[j] - depth[i]);
		}
		for(int i=j+1; i<4; i++) {
			score[i] += g1(vec2(offsets[j]-offsets[i]).length()) * g2(1.0 - dot(normal[j], normal[i])) * g3(depth[j] - depth[i]);
		}
	}
	for(int j=4; j<16; j++) {
		ivec2 p_j = center + offsets[j];

		float d = texelFetch(depth_sampler,                  p_j, 0).r;
		vec3  n = decode_normal(texelFetch(mat_data_sampler, p_j, 0).rg);

		for(int i=0; i<4; i++) {
			score[i] += g1(vec2(offsets[j]).length()) * g2(1.0 - dot(n, normal[i])) * g3(d - depth[i]);
		}
	}


	float top_score = score[0];
	int top_index = 0;
	for(int i=1; i<4; i++) {
		if(score[i] > top_score) {
			top_score = score[i];
			top_index = i;
		}
	}

	out_depth    = vec4(depth[top_index], 0,0,1);
	out_mat_data = mat_data[top_index];
*/
//	out_depth = textureLod(depth_sampler, vertex_out.tex_coords, src_lod);
//	out_mat_data = textureLod(mat_data_sampler, vertex_out.tex_coords, 0);
}
