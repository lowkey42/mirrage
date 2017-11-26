#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable


layout(location = 0) in Vertex_data {
	vec2 tex_coords;
} vertex_out;

layout(location = 0) out vec4 out_depth;
layout(location = 1) out vec4 out_mat_data;

layout(set=1, binding = 0) uniform sampler2D depth_sampler;
layout(set=1, binding = 1) uniform sampler2D mat_data_sampler;

// gaussian weight for normal axis difference
float g1(float x) {
	float b = 0;
	float c = 0.1;
	return exp(- (x-b)*(x-b) / (2*c*c));
}

// gaussian weight for depth difference
float g2(float x) {
	float b = 0;
	float c = 0.001f;
	return exp(- (x-b)*(x-b) / (2*c*c));
}

// finds the pixel of the 4 high-res pixels that is most similar to their surrounding 16 pixels
void main() {
	// calculate uv coordinates of 2x2 blocks to sample
	vec2 tex_size = textureSize(depth_sampler, 0);
	const vec2 uv_00 = vertex_out.tex_coords + vec2(-1,-1) / tex_size;
	const vec2 uv_10 = vertex_out.tex_coords + vec2( 1,-1) / tex_size;
	const vec2 uv_11 = vertex_out.tex_coords + vec2( 1, 1) / tex_size;
	const vec2 uv_01 = vertex_out.tex_coords + vec2(-1, 1) / tex_size;
	const ivec2[] center_offsets = ivec2[4](ivec2(0,0), ivec2(1,0), ivec2(1,1), ivec2(0,1));

	// sample depth and calculate score based on their difference to the center depth value
	vec4 depth_00 = textureGather(depth_sampler, uv_00, 0);
	vec4 depth_10 = textureGather(depth_sampler, uv_10, 0);
	vec4 depth_11 = textureGather(depth_sampler, uv_11, 0);
	vec4 depth_01 = textureGather(depth_sampler, uv_01, 0);

	float avg_depth = (dot(vec4(1), depth_00) + dot(vec4(1), depth_10)
	                 + dot(vec4(1), depth_11) + dot(vec4(1), depth_01)) / 16.0;

	vec4 center_depths = vec4(depth_00.y, depth_10.x, depth_11.w, depth_01.z);

	vec4 score = vec4(1.0);
	score *= vec4(g2(avg_depth - center_depths.x),
	              g2(avg_depth - center_depths.y),
	              g2(avg_depth - center_depths.z),
	              g2(avg_depth - center_depths.w) );

	// sample x axis of encoded normals and modulate score based on their difference to center value
	vec4 normal_x_00 = textureGather(mat_data_sampler, uv_00, 0);
	vec4 normal_x_10 = textureGather(mat_data_sampler, uv_10, 0);
	vec4 normal_x_11 = textureGather(mat_data_sampler, uv_11, 0);
	vec4 normal_x_01 = textureGather(mat_data_sampler, uv_01, 0);

	float avg_normal_x = (dot(vec4(1), normal_x_00) + dot(vec4(1), normal_x_10)
	                    + dot(vec4(1), normal_x_11) + dot(vec4(1), normal_x_01)) / 16.0;

	vec4 center_normals_x = vec4(normal_x_00.y, normal_x_10.x, normal_x_11.w, normal_x_01.z);

	score *= vec4(g1(avg_normal_x - center_normals_x.x),
	              g1(avg_normal_x - center_normals_x.y),
	              g1(avg_normal_x - center_normals_x.z),
	              g1(avg_normal_x - center_normals_x.w) );

	// sample y axis of encoded normals and modulate score based on their difference to center value
	vec4 normal_y_00 = textureGather(mat_data_sampler, uv_00, 1);
	vec4 normal_y_10 = textureGather(mat_data_sampler, uv_10, 1);
	vec4 normal_y_11 = textureGather(mat_data_sampler, uv_11, 1);
	vec4 normal_y_01 = textureGather(mat_data_sampler, uv_01, 1);

	float avg_normal_y = (dot(vec4(1), normal_y_00) + dot(vec4(1), normal_y_10)
	                    + dot(vec4(1), normal_y_11) + dot(vec4(1), normal_y_01)) / 16.0;

	vec4 center_normals_y = vec4(normal_y_00.y, normal_y_10.x, normal_y_11.w, normal_y_01.z);

	score *= vec4(g1(avg_normal_y - center_normals_y.x),
	              g1(avg_normal_y - center_normals_y.y),
	              g1(avg_normal_y - center_normals_y.z),
	              g1(avg_normal_y - center_normals_y.w) );

	// determine the index of the pixel with the highes score
	int max_index = 3;
	float s = score.w;

	if(score.x > s) {
		max_index = 0;
		s = score.x;
	}
	if(score.y > s) {
		max_index = 1;
		s = score.y;
	}
	if(score.z > s) {
		max_index = 2;
		s = score.z;
	}

	// write the depth/mat_data that is most similar to its surroundings
	out_depth    = texelFetch(depth_sampler,    ivec2(vertex_out.tex_coords * tex_size) + center_offsets[max_index], 0);
	out_mat_data = texelFetch(mat_data_sampler, ivec2(vertex_out.tex_coords * tex_size) + center_offsets[max_index], 0);
}
