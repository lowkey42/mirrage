#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "normal_encoding.glsl"
#include "color_conversion.glsl"


layout(location = 0) in Vertex_data {
	vec2 tex_coords;
} vertex_out;

layout(location = 0) out vec4 out_color;

layout(set=1, binding = 0) uniform sampler2D color_sampler;
layout(set=1, binding = 1) uniform sampler2D avg_log_luminance_sampler;
layout(set=1, binding = 2) uniform sampler2D bloom_sampler;

layout(push_constant) uniform Settings {
	vec4 options;
} pcs;


vec3 saturation(vec3 c, float change) {
	vec3 f = vec3(0.299,0.587,0.114);
	float p = sqrt(c.r*c.r*f.r + c.g*c.g*f.g + c.b*c.b*f.b);

	return vec3(p) + (c-vec3(p))*vec3(change);
}

vec3 heji_dawson(vec3 color) {
	vec3 X = max(vec3(0.0), color-0.004);
	vec3 mapped = (X*(6.2*X+.5))/(X*(6.2*X+1.7)+0.06);
	return mapped * mapped;
}

vec3 ToneMapFilmicALU(vec3 color) {
    color = max(vec3(0), color - 0.004);
    color = (color * (6.2 * color + 0.5)) / (color * (6.2 * color + 1.7)+ 0.06);

    // result has 1/2.2 baked in
    return pow(color, vec3(2.2));
}

vec3 expose(vec3 color, float threshold) {
	float avg_luminance = max(exp(texture(avg_log_luminance_sampler, vec2(0.5, 0.5)).r), 0.001);

	float key = 1.03f - (2.0f / (2 + log(avg_luminance + 1)/log(10)));
	float exposure = clamp(key/avg_luminance, 0.1, 3.0) + 0.5;

	if(pcs.options.z>0) {
		exposure = pcs.options.z;
	}

	exposure = log2(exposure);
	exposure -= threshold;

	return color * exp2(exposure);
}

vec3 tone_mapping(vec3 color) {
	if(pcs.options.x<=0.1)
		return color;

	// float exposure = settings.options.r;
	color = expose(color, 0);
	//color = heji_dawson(color);
	color = ToneMapFilmicALU(color);

	if(pcs.options.y >= 0.01) {
		color += textureLod(bloom_sampler, vertex_out.tex_coords, 0).rgb * pcs.options.y;
	}

	return color;
}

vec3 resolve_fxaa() {
	float FXAA_SPAN_MAX = 8.0;
	float FXAA_REDUCE_MUL = 1.0/8.0;
	float FXAA_REDUCE_MIN = 1.0/128.0;

	vec2 texture_size = textureSize(color_sampler, 0);
	
	vec3 rgbNW=textureLod(color_sampler,vertex_out.tex_coords+(vec2(-1.0,-1.0)/texture_size), 0.0).xyz;
	vec3 rgbNE=textureLod(color_sampler,vertex_out.tex_coords+(vec2(1.0,-1.0)/texture_size), 0.0).xyz;
	vec3 rgbSW=textureLod(color_sampler,vertex_out.tex_coords+(vec2(-1.0,1.0)/texture_size), 0.0).xyz;
	vec3 rgbSE=textureLod(color_sampler,vertex_out.tex_coords+(vec2(1.0,1.0)/texture_size), 0.0).xyz;
	vec3 rgbM=textureLod(color_sampler, vertex_out.tex_coords, 0.0).xyz;

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
		textureLod(color_sampler, vertex_out.tex_coords.xy + dir * (1.0/3.0 - 0.5), 0.0).xyz +
		textureLod(color_sampler, vertex_out.tex_coords.xy + dir * (2.0/3.0 - 0.5), 0.0).xyz);
	vec3 rgbB = rgbA * (1.0/2.0) + (1.0/4.0) * (
		textureLod(color_sampler, vertex_out.tex_coords.xy + dir * (0.0/3.0 - 0.5), 0.0).xyz +
		textureLod(color_sampler, vertex_out.tex_coords.xy + dir * (3.0/3.0 - 0.5), 0.0).xyz);
	float lumaB = dot(rgbB, luma);

	if((lumaB < lumaMin) || (lumaB > lumaMax)){
		return rgbA;
	}else{
		return rgbB;
	}
}

void main() {
	//out_color = vec4(tone_mapping(texture(color_sampler, vertex_out.tex_coords).rgb), 1.0);
	out_color = vec4(tone_mapping(resolve_fxaa().rgb), 1.0);
}
