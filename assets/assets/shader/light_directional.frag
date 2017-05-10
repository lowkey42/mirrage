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

layout(set=2, binding = 0) uniform texture2D shadowmaps[1];
layout(set=2, binding = 1) uniform samplerShadow shadowmap_shadow_sampler; // sampler2DShadow
layout(set=2, binding = 2) uniform sampler shadowmap_depth_sampler; // sampler2D

layout (constant_id = 0) const int SHADOW_QUALITY = 1;

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
	float shadow = sample_shadowmap(position + N*0.4);
	out_color = vec4((kD * albedo / PI + brdf) * radiance * NdotL * shadow, 1.0);

	// TODO: remove ambient
	out_color.rgb += albedo * radiance * 0.003;
}

// A single iteration of Bob Jenkins' One-At-A-Time hashing algorithm.
uint hash( uint x ) {
    x += ( x << 10u );
    x ^= ( x >>  6u );
    x += ( x <<  3u );
    x ^= ( x >> 11u );
    x += ( x << 15u );
    return x;
}



// Compound versions of the hashing algorithm I whipped together.
uint hash( uvec2 v ) { return hash( v.x ^ hash(v.y)                         ); }
uint hash( uvec3 v ) { return hash( v.x ^ hash(v.y) ^ hash(v.z)             ); }
uint hash( uvec4 v ) { return hash( v.x ^ hash(v.y) ^ hash(v.z) ^ hash(v.w) ); }



// Construct a float with half-open range [0:1] using low 23 bits.
// All zeroes yields 0.0, all ones yields the next smallest representable value below 1.0.
float floatConstruct( uint m ) {
    const uint ieeeMantissa = 0x007FFFFFu; // binary32 mantissa bitmask
    const uint ieeeOne      = 0x3F800000u; // 1.0 in IEEE binary32

    m &= ieeeMantissa;                     // Keep only mantissa bits (fractional part)
    m |= ieeeOne;                          // Add fractional part to 1.0

    float  f = uintBitsToFloat( m );       // Range [1:2]
    return f - 1.0;                        // Range [0:1]
}

float random(vec4 seed) {
	return floatConstruct(hash(floatBitsToUint(seed)));
}

vec2 PDnrand2( vec4 n ) {
	return fract( sin(dot(n, vec4(12.9898,78.233,45.164,94.673)))* vec2(43758.5453f, 28001.8384f) );
}

const vec2 Poisson4[4] = vec2[](
	vec2( -0.94201624, -0.39906216 ),
	vec2( 0.94558609, -0.76890725 ),
	vec2( -0.094184101, -0.92938870 ),
	vec2( 0.34495938, 0.29387760 )
);
const vec2 Poisson16[16] = vec2[](
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

const vec2 Poisson128[128] = vec2[](
    vec2(-0.9406119, 0.2160107),
    vec2(-0.920003, 0.03135762),
    vec2(-0.917876, -0.2841548),
    vec2(-0.9166079, -0.1372365),
    vec2(-0.8978907, -0.4213504),
    vec2(-0.8467999, 0.5201505),
    vec2(-0.8261013, 0.3743192),
    vec2(-0.7835162, 0.01432008),
    vec2(-0.779963, 0.2161933),
    vec2(-0.7719588, 0.6335353),
    vec2(-0.7658782, -0.3316436),
    vec2(-0.7341912, -0.5430729),
    vec2(-0.6825727, -0.1883408),
    vec2(-0.6777467, 0.3313724),
    vec2(-0.662191, 0.5155144),
    vec2(-0.6569989, -0.7000636),
    vec2(-0.6021447, 0.7923283),
    vec2(-0.5980815, -0.5529259),
    vec2(-0.5867089, 0.09857152),
    vec2(-0.5774597, -0.8154474),
    vec2(-0.5767041, -0.2656419),
    vec2(-0.575091, -0.4220052),
    vec2(-0.5486979, -0.09635002),
    vec2(-0.5235587, 0.6594529),
    vec2(-0.5170338, -0.6636339),
    vec2(-0.5114055, 0.4373561),
    vec2(-0.4844725, 0.2985838),
    vec2(-0.4803245, 0.8482798),
    vec2(-0.4651957, -0.5392771),
    vec2(-0.4529685, 0.09942394),
    vec2(-0.4523471, -0.3125569),
    vec2(-0.4268422, 0.5644538),
    vec2(-0.4187512, -0.8636028),
    vec2(-0.4160798, -0.0844868),
    vec2(-0.3751733, 0.2196607),
    vec2(-0.3656596, -0.7324334),
    vec2(-0.3286595, -0.2012637),
    vec2(-0.3147397, -0.0006635741),
    vec2(-0.3135846, 0.3636878),
    vec2(-0.3042951, -0.4983553),
    vec2(-0.2974239, 0.7496996),
    vec2(-0.2903037, 0.8890813),
    vec2(-0.2878664, -0.8622097),
    vec2(-0.2588971, -0.653879),
    vec2(-0.2555692, 0.5041648),
    vec2(-0.2553292, -0.3389159),
    vec2(-0.2401368, 0.2306108),
    vec2(-0.2124457, -0.09935001),
    vec2(-0.1877905, 0.1098409),
    vec2(-0.1559879, 0.3356432),
    vec2(-0.1499449, 0.7487829),
    vec2(-0.146661, -0.9256138),
    vec2(-0.1342774, 0.6185387),
    vec2(-0.1224529, -0.3887629),
    vec2(-0.116467, 0.8827716),
    vec2(-0.1157598, -0.539999),
    vec2(-0.09983152, -0.2407187),
    vec2(-0.09953719, -0.78346),
    vec2(-0.08604223, 0.4591112),
    vec2(-0.02128129, 0.1551989),
    vec2(-0.01478849, 0.6969455),
    vec2(-0.01231739, -0.6752576),
    vec2(-0.005001599, -0.004027164),
    vec2(0.00248426, 0.567932),
    vec2(0.00335562, 0.3472346),
    vec2(0.009554717, -0.4025437),
    vec2(0.02231783, -0.1349781),
    vec2(0.04694207, -0.8347212),
    vec2(0.05412609, 0.9042216),
    vec2(0.05812819, -0.9826952),
    vec2(0.1131321, -0.619306),
    vec2(0.1170737, 0.6799788),
    vec2(0.1275105, 0.05326218),
    vec2(0.1393405, -0.2149568),
    vec2(0.1457873, 0.1991508),
    vec2(0.1474208, 0.5443151),
    vec2(0.1497117, -0.3899909),
    vec2(0.1923773, 0.3683496),
    vec2(0.2110928, -0.7888536),
    vec2(0.2148235, 0.9586087),
    vec2(0.2152219, -0.1084362),
    vec2(0.2189204, -0.9644538),
    vec2(0.2220028, -0.5058427),
    vec2(0.2251696, 0.779461),
    vec2(0.2585723, 0.01621339),
    vec2(0.2612841, -0.2832426),
    vec2(0.2665483, -0.6422054),
    vec2(0.2939872, 0.1673226),
    vec2(0.3235748, 0.5643662),
    vec2(0.3269232, 0.6984669),
    vec2(0.3425438, -0.1783788),
    vec2(0.3672505, 0.4398117),
    vec2(0.3755714, -0.8814359),
    vec2(0.379463, 0.2842356),
    vec2(0.3822978, -0.381217),
    vec2(0.4057849, -0.5227674),
    vec2(0.4168737, -0.6936938),
    vec2(0.4202749, 0.8369391),
    vec2(0.4252189, 0.03818182),
    vec2(0.4445904, -0.09360636),
    vec2(0.4684285, 0.5885228),
    vec2(0.4952184, -0.2319764),
    vec2(0.5072351, 0.3683765),
    vec2(0.5136194, -0.3944138),
    vec2(0.519893, 0.7157083),
    vec2(0.5277841, 0.1486474),
    vec2(0.5474944, -0.7618791),
    vec2(0.5692734, 0.4852227),
    vec2(0.582229, -0.5125455),
    vec2(0.583022, 0.008507785),
    vec2(0.6500257, 0.3473313),
    vec2(0.6621304, -0.6280518),
    vec2(0.6674218, -0.2260806),
    vec2(0.6741871, 0.6734863),
    vec2(0.6753459, 0.1119422),
    vec2(0.7083091, -0.4393666),
    vec2(0.7106963, -0.102099),
    vec2(0.7606754, 0.5743545),
    vec2(0.7846709, 0.2282225),
    vec2(0.7871446, 0.3891495),
    vec2(0.8071781, -0.5257092),
    vec2(0.8230689, 0.002674922),
    vec2(0.8531976, -0.3256475),
    vec2(0.8758298, -0.1824844),
    vec2(0.8797691, 0.1284946),
    vec2(0.926309, 0.3576975),
    vec2(0.9608918, -0.03495717),
    vec2(0.972032, 0.2271516)
);


float calc_penumbra(vec3 surface_lightspace, float light_size,
                    out int num_occluders);

float sample_shadowmap(vec3 world_pos) {
	int shadowmap = int(model_uniforms.light_data2.r);
	if(shadowmap<0)
		return 1.0;

	vec4 lightspace_pos = model_uniforms.model * vec4(world_pos, 1.0);
	lightspace_pos /= lightspace_pos.w;
	lightspace_pos.xy = lightspace_pos.xy * 0.5 + 0.5;

	float shadowmap_size = textureSize(sampler2D(shadowmaps[shadowmap], shadowmap_depth_sampler), 0).x;
	float light_size = model_uniforms.light_data.r / 800.0;

	int num_occluders;
	float penumbra_softness = calc_penumbra(lightspace_pos.xyz, light_size, num_occluders);

	//return penumbra_softness>=0.5 ? 1.0 : 0.0;

	if(num_occluders==0)
		return 1.0;
	else if(SHADOW_QUALITY<=0 && num_occluders>=4)
		return 0.0;
	
	float sample_size = mix(1.0/shadowmap_size, light_size, penumbra_softness);
	int samples = int(mix(4, 16, penumbra_softness));
	if(SHADOW_QUALITY<=1)
		samples = min(samples, 8);

	float z_bias = 0.002;

	float visiblity = 1.0;
	for (int i=0;i<samples;i++) {
		vec2 offset = (SHADOW_QUALITY<=1) ? Poisson16[int(random(vec4(world_pos, float(i))) * 16) % 16]
		                                  : Poisson128[int(random(vec4(world_pos, float(i))) * 128) % 128];
		
		vec2 p = lightspace_pos.xy + offset * sample_size;

		visiblity -= 1.0/samples * (1.0 - texture(sampler2DShadow(shadowmaps[shadowmap], shadowmap_shadow_sampler),
		                                     vec3(p, lightspace_pos.z-z_bias)));
	}

	return clamp(visiblity, 0.0, 1.0);
}

float calc_avg_occluder(vec3 surface_lightspace, float search_area,
                        out int num_occluders) {
	int shadowmap = int(model_uniforms.light_data2.r);

	float depth_acc;
	float depth_count;
	num_occluders = 0;

	for (int i=0;i<4;i++) {
		vec2 offset = Poisson16[int(random(vec4(surface_lightspace, float(i))) * 16) % 16];
		
		float depth = texture(sampler2D(shadowmaps[shadowmap], shadowmap_depth_sampler),
		                      surface_lightspace.xy + offset * search_area).r;
		if(depth < surface_lightspace.z - 0.002) {
			depth_acc += depth;
			depth_count += 1.0;
			num_occluders += 1;
		}
	}

	if(num_occluders==0)
		return 1.0;

	return depth_acc / depth_count;
}

float calc_penumbra(vec3 surface_lightspace, float light_size, out int num_occluders) {
	float avg_occluder = calc_avg_occluder(surface_lightspace, light_size, num_occluders);

	const float scale = 8.0;
	float softness = (surface_lightspace.z - avg_occluder) * scale;

	return smoothstep(0.1, 1.0, softness);
}
