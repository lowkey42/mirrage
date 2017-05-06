#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "normal_encoding.glsl"


layout(location = 0) in Vertex_data {
	vec2 tex_coords;
} vertex_out;

layout(location = 0) out vec4 out_color;

layout(set=1, binding = 0) uniform sampler2D color_sampler;

layout(push_constant) uniform Settings {
	vec4 options;
} settings;


vec3 heji_dawson(vec3 color) {
	vec3 X = max(vec3(0.0), color-0.004);
	vec3 mapped = (X*(6.2*X+.5))/(X*(6.2*X+1.7)+0.06);
	return mapped * mapped;
}

vec3 tone_mapping(vec3 color) {
	float exposure = settings.options.r;
	color *= exposure;
	color = heji_dawson(color);

	return color;
}

float luminance(vec3 c) {
	vec3 f = vec3(0.299,0.587,0.114);
	return sqrt(c.r*c.r*f.r + c.g*c.g*f.g + c.b*c.b*f.b);
}
vec3 resolve_fxaa() {
	float FXAA_SPAN_MAX = 8.0;
	float FXAA_REDUCE_MUL = 1.0/8.0;
	float FXAA_REDUCE_MIN = 1.0/128.0;

	vec2 texture_size = textureSize(color_sampler, 0);
	
	vec3 rgbNW=texture(color_sampler,vertex_out.tex_coords+(vec2(-1.0,-1.0)/texture_size)).xyz;
	vec3 rgbNE=texture(color_sampler,vertex_out.tex_coords+(vec2(1.0,-1.0)/texture_size)).xyz;
	vec3 rgbSW=texture(color_sampler,vertex_out.tex_coords+(vec2(-1.0,1.0)/texture_size)).xyz;
	vec3 rgbSE=texture(color_sampler,vertex_out.tex_coords+(vec2(1.0,1.0)/texture_size)).xyz;
	vec3 rgbM=texture(color_sampler, vertex_out.tex_coords).xyz;

	vec3 luma=vec3(0.299, 0.587, 0.114);
	float lumaNW = dot(rgbNW, luma);
	float lumaNE = dot(rgbNE, luma);
	float lumaSW = dot(rgbSW, luma);
	float lumaSE = dot(rgbSE, luma);
	float lumaM  = dot(rgbM,  luma);

	float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
	float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));

	vec2 dir;
	dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
	dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));

	float dirReduce = max(
		(lumaNW + lumaNE + lumaSW + lumaSE) * (0.25 * FXAA_REDUCE_MUL),
		FXAA_REDUCE_MIN);

	float rcpDirMin = 1.0/(min(abs(dir.x), abs(dir.y)) + dirReduce);

	dir = min(vec2( FXAA_SPAN_MAX,  FXAA_SPAN_MAX),
	      max(vec2(-FXAA_SPAN_MAX, -FXAA_SPAN_MAX),
	      dir * rcpDirMin)) / texture_size;

	vec3 rgbA = (1.0/2.0) * (
		texture(color_sampler, vertex_out.tex_coords.xy + dir * (1.0/3.0 - 0.5)).xyz +
		texture(color_sampler, vertex_out.tex_coords.xy + dir * (2.0/3.0 - 0.5)).xyz);
	vec3 rgbB = rgbA * (1.0/2.0) + (1.0/4.0) * (
		texture(color_sampler, vertex_out.tex_coords.xy + dir * (0.0/3.0 - 0.5)).xyz +
		texture(color_sampler, vertex_out.tex_coords.xy + dir * (3.0/3.0 - 0.5)).xyz);
	float lumaB = dot(rgbB, luma);

	if((lumaB < lumaMin) || (lumaB > lumaMax)){
		return rgbA;
	}else{
		return rgbB;
	}
}

void main() {
	out_color = vec4(tone_mapping(texture(color_sampler, vertex_out.tex_coords).rgb), 1.0);
}
