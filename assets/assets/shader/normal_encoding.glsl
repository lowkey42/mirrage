
vec2 oct_wrap(vec2 v) {
	return fma(step(vec2(0.0), v), vec2(2.0), vec2(-1.0));
}
vec2 encode_normal(vec3 n) {
	n.xy /= dot(abs(n), vec3(1.0));
	return mix(n.xy, (1.0 - abs(n.yx)) * oct_wrap(n.xy), step(n.z, 0.0)) * 0.5 + 0.5;
}
vec3 decode_normal(vec2 en) {
	en = en*2.0 - 1.0;

	vec3 n = vec3(en.xy, 1.0 - abs(en.x) - abs(en.y));
	if(n.z < 0 )
		n.xy = (1.0 - abs(n.yx)) * oct_wrap(n.xy);

	return normalize(n);
}
