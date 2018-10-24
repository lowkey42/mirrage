#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable


layout(location = 0) in Vertex_data {
	vec2 tex_coords;
} vertex_out;

layout(location = 0) out vec4 out_color;

layout(set=1, binding = 0) uniform sampler2D color_sampler;

// A Fast, Small-Radius GPU Median Filter by Morgan McGuire: http://casual-effects.com/research/McGuire2008Median/index.html
#define s2(a, b)				temp = a; a = min(a, b); b = max(temp, b);
#define mn3(a, b, c)			s2(a, b); s2(a, c);
#define mx3(a, b, c)			s2(b, c); s2(a, c);

#define mnmx3(a, b, c)			mx3(a, b, c); s2(a, b);                                   // 3 exchanges
#define mnmx4(a, b, c, d)		s2(a, b); s2(c, d); s2(a, c); s2(b, d);                   // 4 exchanges
#define mnmx5(a, b, c, d, e)	s2(a, b); s2(c, d); mn3(a, c, e); mx3(b, d, e);           // 6 exchanges
#define mnmx6(a, b, c, d, e, f) s2(a, d); s2(b, e); s2(c, f); mn3(a, b, c); mx3(d, e, f); // 7 exchanges

void main() {
	ivec2 result_sampler_size = textureSize(color_sampler, 0).xy;
	ivec2 uv = ivec2(textureSize(color_sampler, 0).xy * vertex_out.tex_coords);
	vec3 colors[9];
	for(int x=-1; x<=1; x++) {
		for(int y=-1; y<=1; y++) {
			ivec2 c_uv = uv+ivec2(x,y);
			c_uv = clamp(c_uv, ivec2(0,0), result_sampler_size-ivec2(1,1));
			colors[(x+1)*3+(y+1)] = texelFetch(color_sampler, c_uv, 0).rgb;
		}
	}

	float min_c = dot(colors[0], colors[0]);
	float max_c = min_c;

	for(int i=1; i<9; i++) {
		float intensity = dot(colors[i], colors[i]);
		min_c = min(min_c, intensity);
		max_c = max(max_c, intensity);
	}

	vec3 org = colors[4];
	float org_intensity = dot(org, org);
	if(min_c<org_intensity && max_c>org_intensity) {
		out_color = vec4(org, 1.0);

	} else {
		// Starting with a subset of size 6, remove the min and max each time
		vec3 temp;
		mnmx6(colors[0], colors[1], colors[2], colors[3], colors[4], colors[5]);
		mnmx5(colors[1], colors[2], colors[3], colors[4], colors[6]);
		mnmx4(colors[2], colors[3], colors[4], colors[7]);
		mnmx3(colors[3], colors[4], colors[8]);
		out_color = vec4(colors[4], 1.0);
	}
	//out_color = vec4(org, 1.0);
}
