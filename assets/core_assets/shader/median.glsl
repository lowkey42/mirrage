#ifndef MEDIAN_INCLUDED
#define MEDIAN_INCLUDED


// A Fast, Small-Radius GPU Median Filter by Morgan McGuire: http://casual-effects.com/research/McGuire2008Median/index.html
#define s2(a, b)				temp = a; a = min(a, b); b = max(temp, b);
#define mn3(a, b, c)			s2(a, b); s2(a, c);
#define mx3(a, b, c)			s2(b, c); s2(a, c);

#define mnmx3(a, b, c)			mx3(a, b, c); s2(a, b);                                   // 3 exchanges
#define mnmx4(a, b, c, d)		s2(a, b); s2(c, d); s2(a, c); s2(b, d);                   // 4 exchanges
#define mnmx5(a, b, c, d, e)	s2(a, b); s2(c, d); mn3(a, c, e); mx3(b, d, e);           // 6 exchanges
#define mnmx6(a, b, c, d, e, f) s2(a, d); s2(b, e); s2(c, f); mn3(a, b, c); mx3(d, e, f); // 7 exchanges

vec3 median_vec3(inout vec3 data[9]) {
	vec3 temp;
	mnmx6(data[0], data[1], data[2], data[3], data[4], data[5]);
	mnmx5(data[1], data[2], data[3], data[4], data[6]);
	mnmx4(data[2], data[3], data[4], data[7]);
	mnmx3(data[3], data[4], data[8]);
	return data[4];
}

float median_float(inout float data[9]) {
	float temp;
	mnmx6(data[0], data[1], data[2], data[3], data[4], data[5]);
	mnmx5(data[1], data[2], data[3], data[4], data[6]);
	mnmx4(data[2], data[3], data[4], data[7]);
	mnmx3(data[3], data[4], data[8]);
	return data[4];
}

#endif
