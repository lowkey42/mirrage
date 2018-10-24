#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location = 0) out Vertex_data {
	vec2 uv_center;
	vec2 uv_l1;
	vec2 uv_l2;
	vec2 uv_l3;
	vec2 uv_r1;
	vec2 uv_r2;
	vec2 uv_r3;
} vertex_out;

out gl_PerVertex {
	vec4 gl_Position;
};

layout (constant_id = 0) const int HORIZONTAL = 1;

layout(set=1, binding = 1) uniform sampler2D color_sampler;


void main() {
	vec2 uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	gl_Position = vec4(uv * 2.0f + -1.0f, 0.0f, 1.0f);

	vec2 size = textureSize(color_sampler, 0);

	vec2 tex_offset = (HORIZONTAL==1 ? vec2(1,0) : vec2(0,1)) / size;

	vertex_out.uv_center = uv;

	vertex_out.uv_l1 = uv + 1.411764705882353 * tex_offset;
	vertex_out.uv_l2 = uv + 3.2941176470588234* tex_offset;
	vertex_out.uv_l3 = uv + 5.176470588235294 * tex_offset;

	vertex_out.uv_r1 = uv - 1.411764705882353 * tex_offset;
	vertex_out.uv_r2 = uv - 3.2941176470588234* tex_offset;
	vertex_out.uv_r3 = uv - 5.176470588235294 * tex_offset;
}
