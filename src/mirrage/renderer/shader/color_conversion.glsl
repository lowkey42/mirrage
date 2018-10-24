#ifndef COLOR_CONVERSION_INCLUDED
#define COLOR_CONVERSION_INCLUDED


// source: http://lolengine.net/blog/2013/07/27/rgb-to-hsv-in-glsl
vec3 rgb2hsv(vec3 c) {
	vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
	vec4 p = c.g < c.b ? vec4(c.bg, K.wz) : vec4(c.gb, K.xy);
	vec4 q = c.r < p.x ? vec4(p.xyw, c.r) : vec4(c.r, p.yzx);

	float d = q.x - min(q.w, q.y);
	float e = 1.0e-10;
	return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}
vec3 hsv2rgb(vec3 c) {
	vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
	vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
	return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

float luminance(vec3 c) {
	vec3 f = vec3(0.299,0.587,0.114);
	return sqrt(c.r*c.r*f.r + c.g*c.g*f.g + c.b*c.b*f.b);
}

vec3 clamp_color_luminance(vec3 c, float max_luminance) {
	vec3 hsv = rgb2hsv(c);

	hsv.b = min(max_luminance, hsv.b);

	return hsv2rgb(hsv);
}

vec3 rgb2cie(vec3 c) {
	mat3 m = mat3(
		vec3(0.4123908, 0.2126390, 0.01933082),
		vec3(0.3575843, 0.7151687, 0.1191948),
		vec3(0.1804808, 0.07219232, 0.9505322) );
	return m * c;
}
vec3 cie2rgb(vec3 c) {
	mat3 m = mat3(
		vec3(3.240970, -0.9692436, 0.05563008),
		vec3(-1.537383, 1.875968, -0.2039770),
		vec3(-0.4986108, 0.04155506, 1.056972) );
	return m * c;
}

#endif
