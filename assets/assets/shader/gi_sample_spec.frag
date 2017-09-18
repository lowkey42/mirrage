#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable


layout(location = 0) in Vertex_data {
	vec2 tex_coords;
} vertex_out;

layout(location = 0) out vec4 out_color;

layout(set=1, binding = 0) uniform sampler2D color_sampler;
layout(set=1, binding = 1) uniform sampler2D depth_sampler;
layout(set=1, binding = 2) uniform sampler2D mat_data_sampler;
layout(set=1, binding = 3) uniform sampler2D result_sampler;
layout(set=1, binding = 4) uniform sampler2D history_weight_sampler;

layout(push_constant) uniform Push_constants {
	mat4 projection;
	mat4 prev_projection;
} pcs;


#include "global_uniforms.glsl"
#include "normal_encoding.glsl"
#include "random.glsl"
#include "brdf.glsl"
#include "upsample.glsl"
#include "raycast.glsl"

const float PI = 3.14159265359;


float luminance_norm(vec3 c) {
	vec3 f = vec3(0.299,0.587,0.114);
	return sqrt(c.r*c.r*f.r + c.g*c.g*f.g + c.b*c.b*f.b);
}

// losely based on https://www.gamedev.net/topic/658702-help-with-gpu-pro-5-hi-z-screen-space-reflections/?view=findpost&p=5173175
//   and http://roar11.com/2015/07/screen-space-glossy-reflections/


float roughness_to_spec_lobe_angle(float roughness) {
	// see: http://graphicrants.blogspot.de/2013/08/specular-brdf-reference.html
	float power = clamp(2/max(0.0001, roughness*roughness) - 2, 32.0, 1024*20);

	return acos(pow(0.244, 1.0/(power + 1.0)));
}

float isosceles_triangle_opposite(float adjacentLength, float coneTheta) {
	return 2.0f * tan(coneTheta) * adjacentLength;
}

float isosceles_triangle_inradius(float a, float h) {
	float a2 = a * a;
	float fh2 = 4.0f * h * h;
	return (a * (sqrt(a2 + fh2) - a)) / (4.0f * h);
}

float isosceles_triangle_next_adjacent(float adjacentLength, float incircleRadius) {
	// subtract the diameter of the incircle to get the adjacent side of the next level on the cone
	return adjacentLength - (incircleRadius * 2.0f);
}

vec3 cone_tracing(float roughness, vec2 hit_uv, vec3 L, float coneTheta) {
	float min_lod = pcs.prev_projection[0][3];
	float max_lod = max(min_lod, pcs.prev_projection[1][3]);
	vec2  depth_size  = textureSize(depth_sampler, int(min_lod + 0.5));
	float screen_size = max(depth_size.x, depth_size.y);
	float glossiness = 1.0 - roughness;

	vec2  delta = (hit_uv - vertex_out.tex_coords);
	float adjacent_length = length(delta);
	if(adjacent_length<0.00001)
		return vec3(0,0,0);

	vec2  adjacent_unit   = delta / adjacent_length;

	vec4 color = vec4(0,0,0,0);
	float glossiness_mult = glossiness;
	float remaining_alpha = 1.0;

	for(int i=0; i<14; i++) {
		float opposite_length = isosceles_triangle_opposite(adjacent_length, coneTheta);
		float incircle_size   = isosceles_triangle_inradius(adjacent_length, opposite_length);

		vec2 uv = (vertex_out.tex_coords + adjacent_unit*(adjacent_length - incircle_size));
		float lod = incircle_size<0.00001 ? min_lod : clamp(log2(incircle_size * screen_size)-1, min_lod, max_lod);

		vec4 s = vec4(textureLod(color_sampler, uv, lod).rgb, 1) * glossiness_mult;
		if(lod > max_lod-0.5) {
			s.rgb *= 1.0 - min(0.5, lod-(max_lod-0.5))*2;
		} else {
			int ilod = int(lod + 0.5);
			vec3 N = decode_normal(texelFetch(mat_data_sampler, ivec2(uv*textureSize(mat_data_sampler, ilod)), ilod).rg);
			s.rgb *= clamp(1.0 - dot(L, N), 0, 1);
		}

		remaining_alpha -= s.a;
		if(remaining_alpha < 0.0) {
			s.rgb *= (1.0 - abs(remaining_alpha));
		}
		color += s;

		if(color.a >= 1.0) {
			break;
		}

		adjacent_length = isosceles_triangle_next_adjacent(adjacent_length, incircle_size);
		glossiness_mult *= glossiness;
	}

	return color.rgb * (1 - max(0.0, remaining_alpha));
}

void main() {
	float startLod = pcs.prev_projection[0][3];
	vec2 depthSize = textureSize(depth_sampler, int(startLod + 0.5));

	out_color = vec4(0,0,0,1);

	float depth  = textureLod(depth_sampler, vertex_out.tex_coords, startLod).r;
	vec3 P = position_from_ldepth(vertex_out.tex_coords, depth);

	vec4 mat_data = textureLod(mat_data_sampler, vertex_out.tex_coords, 0);
	vec3 N = decode_normal(mat_data.rg);
	float roughness = mat_data.b;
	float metallic = mat_data.a;

	vec3 V = -normalize(P);

	vec3 dir = -reflect(V, N);
	P += dir*0.1;

	bool spec_visible = metallic>0.01 || (max(0, dot(normalize(V+dir), dir))<0.7);

	// convert to cone angle (maximum extent of the specular lobe aperture)
	// only want half the full cone angle since we're slicing the isosceles triangle in half to get a right triangle
	float coneTheta = roughness_to_spec_lobe_angle(roughness) * 0.5f;

	// calculate max distance based on roughness
	float max_distance = min(32, 4 / (tan(coneTheta)*2));
	float max_steps = max_distance*16;

	vec3 jitter = PDnrand3(vertex_out.tex_coords);

	vec2 raycast_hit_uv;
	vec3 raycast_hit_point;
	if(spec_visible &&
	   traceScreenSpaceRay1(P+dir*0.25, dir, pcs.projection, depth_sampler,
							depthSize, 2.0, global_uniforms.proj_planes.x,
							10, 0.5*jitter.z, max_distance, max_steps, int(startLod + 0.5),
							raycast_hit_uv, raycast_hit_point)) {

		vec3 L = raycast_hit_point - P;
		float L_len = length(L);
		float factor_distance = 1.0 - min(L_len / 10.0, 1.0);

		vec2 hit_uv = raycast_hit_uv/depthSize + jitter.xy * mix(0.001, 0.03, min(L_len / 10.0, 1.0));

		vec3 hit_N = decode_normal(textureLod(mat_data_sampler, hit_uv, 0).rg);

		float factor_normal = 1.0 - smoothstep(0.6, 0.9, abs(dot(N, hit_N)));

		vec3 color = cone_tracing(roughness, hit_uv, dir, coneTheta);

		out_color.rgb = max(color * factor_distance * factor_normal, vec3(0));

		out_color.rgb /= (1 + luminance_norm(out_color.rgb));

	} else {
		out_color.rgb = textureLod(result_sampler, vertex_out.tex_coords, pcs.prev_projection[0][3]).rgb / (PI*PI*2);
	}

	float history_weight = texelFetch(history_weight_sampler,
	                                  ivec2(vertex_out.tex_coords * textureSize(history_weight_sampler, 0)),
	                                  0).r;

	out_color *= 1.0 - (history_weight*0.92);

	out_color = max(out_color, vec4(0));
}

