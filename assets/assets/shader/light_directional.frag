#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "normal_encoding.glsl"


layout(location = 0) in Vertex_data {
	vec2 tex_coords;
} vertex_out;

layout(location = 0) out vec4 out_color;

layout(input_attachment_index = 0, set=1, binding = 0) uniform subpassInput depth_sampler;
layout(input_attachment_index = 1, set=1, binding = 1) uniform subpassInput albedo_mat_id_sampler;
layout(input_attachment_index = 2, set=1, binding = 2) uniform subpassInput mat_data_sampler;

layout(set=2, binding = 0) uniform sampler2D shadowmap_sampler[1];

layout(binding = 0) uniform Global_uniforms {
	mat4 view_proj;
	mat4 inv_view_proj;
	vec4 eye_pos;
	vec4 proj_planes;
	vec4 time;
} global_uniforms;

layout(push_constant) uniform Per_model_uniforms {
	mat4 model;
	vec4 light_color;
	vec4 light_data;  // R=src_radius, GBA=direction
	vec4 light_data2; // R=shadowmapID
} model_uniforms;


const float PI = 3.14159265359;

// TODO: refactor and rewrite as required
float DistributionGGX(vec3 N, vec3 H, float roughness) {
	float a      = roughness*roughness;
	float a2     = a*a;
	float NdotH  = max(dot(N, H), 0.0);
	float NdotH2 = NdotH*NdotH;

	float nom   = a2;
	float denom = (NdotH2 * (a2 - 1.0) + 1.0);
	denom = PI * denom * denom;

	return nom / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness) {
	float r = (roughness + 1.0);
	float k = (r*r) / 8.0;

	float nom   = NdotV;
	float denom = NdotV * (1.0 - k) + k;

	return nom / denom;
}
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
	float NdotV = max(dot(N, V), 0.0);
	float NdotL = max(dot(N, L), 0.0);
	float ggx2  = GeometrySchlickGGX(NdotV, roughness);
	float ggx1  = GeometrySchlickGGX(NdotL, roughness);

	return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
	return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}
// END TODO

float sample_shadowmap(vec3 world_pos);

float linearize_depth(float depth) {
	float near_plane = global_uniforms.proj_planes.x;
	float far_plane = global_uniforms.proj_planes.y;
	return (2.0 * near_plane)
	     / (far_plane + near_plane - depth * (far_plane - near_plane));
}

vec3 restore_position(vec2 uv, float depth) {
	vec4 pos_world = global_uniforms.inv_view_proj * vec4(uv * 2.0 - 1.0, depth, 1.0);
	return pos_world.xyz / pos_world.w;
}


void main() {
	float depth         = subpassLoad(depth_sampler).r;
	vec4  albedo_mat_id = subpassLoad(albedo_mat_id_sampler);
	vec4  mat_data      = subpassLoad(mat_data_sampler);

	float linear_depth = linearize_depth(depth);
	vec3 position = restore_position(vertex_out.tex_coords, depth);
	vec3 V = normalize(global_uniforms.eye_pos.xyz - position);
	vec3 albedo = albedo_mat_id.rgb;
	int  material = int(albedo_mat_id.a*255);

	// material 255 (unlit)
	if(material==255) {
		out_color = vec4(albedo, 1.0);
		return;
	}

	// material 0 (default)
	vec3 N = decode_normal(mat_data.rg);
	float roughness = mat_data.b;
	float metallic = mat_data.a;
	
	// TODO: calc light

	vec3 F0 = mix(vec3(0.04), albedo.rgb, metallic);
	albedo.rgb *= 1.0 - metallic;

	vec3 radiance = model_uniforms.light_color.rgb * model_uniforms.light_color.a;
	vec3 L = model_uniforms.light_data.gba;

	vec3 H = normalize(V+L);

	float NDF = DistributionGGX(N, H, roughness);
	float G   = GeometrySmith(N, V, L, roughness);
	vec3 F    = fresnelSchlick(max(dot(H, L), 0.0), F0);

	vec3 kS = F;
	vec3 kD = vec3(1.0) - kS;

	vec3 nominator    = NDF * G * F;
	float denominator = 4 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.001;
	vec3 brdf = nominator / denominator;

	// add to outgoing radiance Lo
	float NdotL = max(dot(N, L), 0.0);
	float shadow = sample_shadowmap(position);
	out_color = vec4((kD * albedo / PI + brdf) * radiance * NdotL * shadow, 1.0);

	// TODO: remove ambient
	out_color.rgb += albedo * radiance * 0.002;
}

float random(vec4 seed) {
    float dot_product = dot(seed, vec4(12.9898,78.233,45.164,94.673));
    return fract(sin(dot_product) * 43758.5453);
}

vec2 PDnrand2( vec4 n ) {
	return fract( sin(dot(n, vec4(12.9898,78.233,45.164,94.673)))* vec2(43758.5453f, 28001.8384f) );
}

float sample_shadowmap(vec3 world_pos) {
	int shadowmap = int(model_uniforms.light_data2.r);
	if(shadowmap<0)
		return 1.0;

	vec4 lightspace_pos = model_uniforms.model * vec4(world_pos, 1.0);
	lightspace_pos /= lightspace_pos.w;

	vec2 poissonDisk[16] = vec2[](
	        vec2( -0.94201624, -0.39906216 ),
			vec2( 0.94558609, -0.76890725 ),
			vec2( -0.094184101, -0.92938870 ),
			vec2( 0.34495938, 0.29387760 ),
			vec2( -0.91588581, 0.45771432 ),
			vec2( -0.81544232, -0.87912464 ),
			vec2( -0.38277543, 0.27676845 ),
			vec2( 0.97484398, 0.75648379 ),
			vec2( 0.44323325, -0.97511554 ),
			vec2( 0.53742981, -0.47373420 ),
			vec2( -0.26496911, -0.41893023 ),
			vec2( 0.79197514, 0.19090188 ),
			vec2( -0.24188840, 0.99706507 ),
			vec2( -0.81409955, 0.91437590 ),
			vec2( 0.19984126, 0.78641367 ),
			vec2( 0.14383161, -0.14100790 )
	);

	float visiblity = 1.0;
	for (int i=0;i<8;i++) {
		int idx = int(random(vec4(world_pos, float(i))) * 16) % 16;
		vec2 p = lightspace_pos.xy*0.5+0.5 + poissonDisk[idx]/700.0;
		//vec2 p = lightspace_pos.xy*0.5+0.5 + PDnrand2(vec4(world_pos, float(i)))/600.0;

		if(texture(shadowmap_sampler[shadowmap], p).r < lightspace_pos.z-0.005)
			visiblity -= 1.0/8;
	}

	return clamp(visiblity, 0.0, 1.0);

	vec2 moments = texture(shadowmap_sampler[shadowmap], lightspace_pos.xy*0.5+0.5).rg;

	if(moments.x < lightspace_pos.z)
		return 0.0;
	else
		return 1.0;

/*
	float depth_exp = exp(120.0 * lightspace_pos.z);
	float x = moments.x / depth_exp;

	return clamp(x, 0.0, 1.0);
*/


/*
	float d = moments.x - lightspace_pos.z;

	if(d>0)
		return 1.0;

	float variance = moments.y - (moments.x*moments.x);
	variance = max(variance, 0.00002);

	float p_max = variance / (variance + d*d);
	p_max = (p_max-0.6) / (1.0-0.6);
	//p_max = p_max*2.0-1.0;

	return clamp(p_max, 0.0, 1.0);
	*/
}
