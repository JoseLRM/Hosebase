#pragma once

#include "Hosebase/defines.h"

#include <math.h>

// Math

#define SV_RADIANS(x) x * 0.0174533f
#define SV_DEGREES(x) x / 0.0174533f
#define PI 3.14159f
#define TAU (2.f*PI)

inline f32 math_sqrt(f32 n)
{
	i32 i;
	f32 x, y;
	x = n * 0.5f;
	y = n;
	i = *(i32*)& y;
	i = 0x5f3759df - (i >> 1);
	y = *(f32*)& i;
	y = y * (1.5f - (x * y * y));
	y = y * (1.5f - (x * y * y));
	return n * y;
}

inline f32 math_sin(f32 n)
{
	return sinf(n);
}

inline f32 math_cos(f32 n)
{
	return cosf(n);
}

inline f32 math_atan2(float a, float b)
{
	return atan2f(a, b);
}

inline f64 math_sqrt_f64(f64 n)
{
	i64 i;
	f64 x, y;
	x = n * 0.5;
	y = n;
	i = *(i64*)& y;
	i = 0x5f3759df - (i >> 1);
	y = *(f64*)& i;
	y = y * (1.5 - (x * y * y));
	y = y * (1.5 - (x * y * y));
	return n * y;
}

inline f32 math_round(f32 n)
{
	i32 i = (i32)n;
	if (n - i >= 0.5f) return i + 1;
	else return i;
}

inline f32 math_clamp(f32 min, f32 n, f32 max)
{
	if (n < min) n = min;
	else if (n > max) n = max;
	return n;
}

inline u32 math_clamp_u32(u32 min, u32 n, u32 max)
{
	if (n < min) n = min;
	else if (n > max) n = max;
	return n;
}

inline f32 math_clamp01(f32 n)
{
	return math_clamp(0.f, n, 1.f);
}

inline f32 math_lerp(f32 t0, f32 t1, f32 n)
{
	return t0 * (1.f - n) + t1 * n;
}

inline f32 math_truncate_high(f32 x)
{
	f32 integer = (f32)((i32)x);
	f32 decimal = x - integer;
	
	if (decimal > 0.0000001f) {
		++integer;
	}
	
	return integer;
}

inline f32 math_sign(f32 n)
{
	return (n < 0.f) ? -1.f : 1.f;
}

inline f32 math_exp(f32 n)
{
	// TODO: 32 bits
	return exp(n);
}

inline f32 math_smooth(f32 n, f32 falloff)
{
	return math_exp(-n * falloff) * (n - 1.f) + 1.f;
}

// Vector

inline v2 v2_set(f32 x, f32 y)
{
	v2 v;
	v.x = x;
	v.y = y;
	return v;
}

inline v2 v2_zero()
{
	v2 v;
	v.x = 0.f;
	v.y = 0.f;
	return v;
}

inline v2 v2_add(v2 a, v2 b)
{
	v2 res;
	res.x = a.x + b.x;
	res.y = a.y + b.y;
	return res;
}

inline v2 v2_sub(v2 a, v2 b)
{
	v2 res;
	res.x = a.x - b.x;
	res.y = a.y - b.y;
	return res;
}

inline v2 v2_mul(v2 a, v2 b)
{
	v2 res;
	res.x = a.x * b.x;
	res.y = a.y * b.y;
	return res;
}

inline v2 v2_div(v2 a, v2 b)
{
	v2 res;
	res.x = a.x / b.x;
	res.y = a.y / b.y;
	return res;
}

inline v2 v2_add_scalar(v2 v, f32 s)
{
	v2 res;
	res.x = v.x + s;
	res.y = v.y + s;
	return res;
}

inline v2 v2_sub_scalar(v2 v, f32 s)
{
	v2 res;
	res.x = v.x - s;
	res.y = v.y - s;
	return res;
}

inline v2 v2_mul_scalar(v2 v, f32 s)
{
	v2 res;
	res.x = v.x * s;
	res.y = v.y * s;
	return res;
}

inline v2 v2_div_scalar(v2 v, f32 s)
{
	v2 res;
	res.x = v.x / s;
	res.y = v.y / s;
	return res;
}
    
inline f32 v2_length(v2 v)
{
	return math_sqrt(v.x * v.x + v.y * v.y);
}

inline v2 v2_normalize(v2 v)
{
	f32 mag = v2_length(v);
	v.x /= mag;
	v.y /= mag;
	return v;
}

inline f32 v2_distance(v2 from, v2 to)
{
	return v2_length(v2_sub(from, to));
}

inline f32 v2_angle(v2 v)
{
	f32 res = math_atan2(v.y, v.x);
	if (res < 0.0) {
		res = (2.f * PI) + res;
	}
	return res;
}
	
inline v2 v2_angle_set(v2 v, f32 angle)
{
	f32 mag = v2_length(v);
	v.x = math_cos(angle);
	v.x *= mag;
	v.y = math_sin(angle);
	v.y *= mag;
	return v;
}

inline f32 v2_dot(v2 a, v2 b)
{
	return a.x * b.x + a.y * b.y;
}

inline v2 v2_perpendicular(v2 v)
{
	v2 r;
	r.x = v.y;
	r.y = -v.x;
	return r;
}

inline v2 v2_reflection(v2 v, v2 normal)
{
	v2 res = v2_sub_scalar(v, 2.f);
	res = v2_mul_scalar(res, v2_dot(v, normal));
	res = v2_mul(res, normal);
	return res;
}

inline v3 v3_set(f32 x, f32 y, f32 z)
{
	v3 v;
	v.x = x;
	v.y = y;
	v.z = z;
	return v;
}

inline v3 v3_zero()
{
	v3 v;
	v.x = 0.f;
	v.y = 0.f;
	v.z = 0.f;
	return v;
}

inline v3 v3_add(v3 a, v3 b)
{
	v3 res;
	res.x = a.x + b.x;
	res.y = a.y + b.y;
	res.z = a.z + b.z;
	return res;
}

inline v3 v3_sub(v3 a, v3 b)
{
	v3 res;
	res.x = a.x - b.x;
	res.y = a.y - b.y;
	res.z = a.z - b.z;
	return res;
}

inline v3 v3_mul(v3 a, v3 b)
{
	v3 res;
	res.x = a.x * b.x;
	res.y = a.y * b.y;
	res.z = a.z * b.z;
	return res;
}

inline v3 v3_div(v3 a, v3 b)
{
	v3 res;
	res.x = a.x / b.x;
	res.y = a.y / b.y;
	res.z = a.z / b.z;
	return res;
}

inline v3 v3_add_scalar(v3 v, f32 s)
{
	v3 res;
	res.x = v.x + s;
	res.y = v.y + s;
	res.z = v.z + s;
	return res;
}

inline v3 v3_sub_scalar(v3 v, f32 s)
{
	v3 res;
	res.x = v.x - s;
	res.y = v.y - s;
	res.z = v.z - s;
	return res;
}

inline v3 v3_mul_scalar(v3 v, f32 s)
{
	v3 res;
	res.x = v.x * s;
	res.y = v.y * s;
	res.z = v.z * s;
	return res;
}

inline v3 v3_div_scalar(v3 v, f32 s)
{
	v3 res;
	res.x = v.x / s;
	res.y = v.y / s;
	res.z = v.z / s;
	return res;
}

inline f32 v3_length(v3 v)
{
	return math_sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

inline v3 v3_normalize(v3 v)
{
	f32 mag = v3_length(v);
	v.x /= mag;
	v.y /= mag;
	v.z /= mag;
	return v;
}

inline f32 v3_distance(v3 from, v3 to)
{
	return v3_length(v3_sub(from, to));
}
	
inline f32 v3_dot(v3 v0, v3 v1)
{
	return v0.x * v1.x + v0.y * v1.y + v0.z * v1.z;
}
	
inline v3 v3_cross(v3 v0, v3 v1)
{
	v3 r;
	r.x = v0.y * v1.z - v0.z * v1.y;
	r.y = v0.z * v1.x - v0.x * v1.z;
	r.z = v0.x * v1.y - v0.y * v1.x;
	return r;
}
	
// NOTE: normal must be normalized
inline v3 v3_reflection(v3 v, v3 normal)
{
	f32 s  = v3_dot(v, normal) * 2.f;
	return v3_mul_scalar(v3_sub(normal, v), s);
}

inline b8 v3_i32_equals(v3_i32 v0, v3_i32 v1)
{
	return v0.x == v1.x && v0.y == v1.y && v0.z == v1.z;
}

inline v4 v4_set(f32 x, f32 y, f32 z, f32 w)
{
	v4 v;
	v.x = x;
	v.y = y;
	v.z = z;
	v.w = w;
	return v;
}

inline v4 v4_zero()
{
	v4 v;
	v.x = 0.f;
	v.y = 0.f;
	v.z = 0.f;
	v.w = 0.f;
	return v;
}

inline v4 v4_add(v4 a, v4 b)
{
	v4 res;
	res.x = a.x + b.x;
	res.y = a.y + b.y;
	res.z = a.z + b.z;
	res.w = a.w + b.w;
	return res;
}

inline v4 v4_sub(v4 a, v4 b)
{
	v4 res;
	res.x = a.x - b.x;
	res.y = a.y - b.y;
	res.z = a.z - b.z;
	res.w = a.w - b.w;
	return res;
}

inline v4 v4_mul(v4 a, v4 b)
{
	v4 res;
	res.x = a.x * b.x;
	res.y = a.y * b.y;
	res.z = a.z * b.z;
	res.w = a.w * b.w;
	return res;
}

inline v4 v4_div(v4 a, v4 b)
{
	v4 res;
	res.x = a.x / b.x;
	res.y = a.y / b.y;
	res.z = a.z / b.z;
	res.w = a.w / b.w;
	return res;
}

inline v4 v4_add_scalar(v4 v, f32 s)
{
	v4 res;
	res.x = v.x + s;
	res.y = v.y + s;
	res.z = v.z + s;
	res.w = v.w + s;
	return res;
}

inline v4 v4_sub_scalar(v4 v, f32 s)
{
	v4 res;
	res.x = v.x - s;
	res.y = v.y - s;
	res.z = v.z - s;
	res.w = v.w - s;
	return res;
}

inline v4 v4_mul_scalar(v4 v, f32 s)
{
	v4 res;
	res.x = v.x * s;
	res.y = v.y * s;
	res.z = v.z * s;
	res.w = v.w * s;
	return res;
}

inline v4 v4_div_scalar(v4 v, f32 s)
{
	v4 res;
	res.x = v.x / s;
	res.y = v.y / s;
	res.z = v.z / s;
	res.w = v.w / s;
	return res;
}

inline f32 v4_length(v4 v)
{
	return math_sqrt(v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w);
}
	
inline v4 v4_normalize(v4 v)
{
	f32 m = v4_length(v);
	v.x /= m;
	v.y /= m;
	v.z /= m;
	v.w /= m;
	return v;
}

inline f32 v4_distance(v4 from, v4 to)
{
	return v4_length(v4_sub(from, to));
}

inline v4 v4_transform(v4 v, m4 m)
{
	v4 r;
	r.x = m.v[0][0] * v.x + m.v[0][1] * v.y + m.v[0][2] * v.z + m.v[0][3] * v.w;
	r.y = m.v[1][0] * v.x + m.v[1][1] * v.y + m.v[1][2] * v.z + m.v[1][3] * v.w;
	r.z = m.v[2][0] * v.x + m.v[2][1] * v.y + m.v[2][2] * v.z + m.v[2][3] * v.w;
	r.w = m.v[3][0] * v.x + m.v[3][1] * v.y + m.v[3][2] * v.z + m.v[3][3] * v.w;
	return r;
}

inline v3 v2_to_v3(v2 v, f32 z)
{
	v3 r;
	r.x = v.x;
	r.y = v.y;
	r.z = z;
	return r;
}

inline v4 v2_to_v4(v2 v, f32 z, f32 w)
{
	v4 r;
	r.x = v.x;
	r.y = v.y;
	r.z = z;
	r.w = w;
	return r;
}

inline v2 v3_to_v2(v3 v)
{
	v2 r;
	r.x = v.x;
	r.y = v.y;
	return r;
}

inline v4 v3_to_v4(v3 v, f32 w)
{
	v4 r;
	r.x = v.x;
	r.y = v.y;
	r.z = v.z;
	r.w = w;
	return r;
}

inline v2 v4_to_v2(v4 v)
{
	v2 r;
	r.x = v.x;
	r.y = v.y;
	return r;
}

inline v3 v4_to_v3(v4 v)
{
	v3 r;
	r.x = v.x;
	r.y = v.y;
	r.z = v.z;
	return r;
}

// Matrix

inline m4 m4_identity()
{
	m4 m;
	m.v[0][0] = 1.f;
	m.v[0][1] = 0.f;
	m.v[0][2] = 0.f;
	m.v[0][3] = 0.f;

	m.v[1][0] = 0.f;
	m.v[1][1] = 1.f;
	m.v[1][2] = 0.f;
	m.v[1][3] = 0.f;

	m.v[2][0] = 0.f;
	m.v[2][1] = 0.f;
	m.v[2][2] = 1.f;
	m.v[2][3] = 0.f;

	m.v[3][0] = 0.f;
	m.v[3][1] = 0.f;
	m.v[3][2] = 0.f;
	m.v[3][3] = 1.f;
	
	return m;
}

inline m4 m4_zero()
{
	m4 m;
	foreach(i, 16)
		m.a[i] = 0.f;
	return m;
}

inline m4 m4_transpose(m4 s)
{
	m4 m;
	m.v[0][0] = s.v[0][0];
	m.v[0][1] = s.v[1][0];
	m.v[0][2] = s.v[2][0];
	m.v[0][3] = s.v[3][0];

	m.v[1][0] = s.v[0][1];
	m.v[1][1] = s.v[1][1];
	m.v[1][2] = s.v[2][1];
	m.v[1][3] = s.v[3][1];

	m.v[2][0] = s.v[0][2];
	m.v[2][1] = s.v[1][2];
	m.v[2][2] = s.v[2][2];
	m.v[2][3] = s.v[3][2];

	m.v[3][0] = s.v[0][3];
	m.v[3][1] = s.v[1][3];
	m.v[3][2] = s.v[2][3];
	m.v[3][3] = s.v[3][3];
	
	return m;
}

inline m4 m4_inverse(m4 m)
{
	// From: https://stackoverflow.com/a/1148405
	
	m4 inv;
	f32 det;
    i32 i;

    inv.a[0] = m.a[5]  * m.a[10] * m.a[15] - 
             m.a[5]  * m.a[11] * m.a[14] - 
             m.a[9]  * m.a[6]  * m.a[15] + 
             m.a[9]  * m.a[7]  * m.a[14] +
             m.a[13] * m.a[6]  * m.a[11] - 
             m.a[13] * m.a[7]  * m.a[10];

    inv.a[4] = -m.a[4]  * m.a[10] * m.a[15] + 
              m.a[4]  * m.a[11] * m.a[14] + 
              m.a[8]  * m.a[6]  * m.a[15] - 
              m.a[8]  * m.a[7]  * m.a[14] - 
              m.a[12] * m.a[6]  * m.a[11] + 
              m.a[12] * m.a[7]  * m.a[10];

    inv.a[8] = m.a[4]  * m.a[9] * m.a[15] - 
             m.a[4]  * m.a[11] * m.a[13] - 
             m.a[8]  * m.a[5] * m.a[15] + 
             m.a[8]  * m.a[7] * m.a[13] + 
             m.a[12] * m.a[5] * m.a[11] - 
             m.a[12] * m.a[7] * m.a[9];

    inv.a[12] = -m.a[4]  * m.a[9] * m.a[14] + 
               m.a[4]  * m.a[10] * m.a[13] +
               m.a[8]  * m.a[5] * m.a[14] - 
               m.a[8]  * m.a[6] * m.a[13] - 
               m.a[12] * m.a[5] * m.a[10] + 
               m.a[12] * m.a[6] * m.a[9];

    inv.a[1] = -m.a[1]  * m.a[10] * m.a[15] + 
              m.a[1]  * m.a[11] * m.a[14] + 
              m.a[9]  * m.a[2] * m.a[15] - 
              m.a[9]  * m.a[3] * m.a[14] - 
              m.a[13] * m.a[2] * m.a[11] + 
              m.a[13] * m.a[3] * m.a[10];

    inv.a[5] = m.a[0]  * m.a[10] * m.a[15] - 
             m.a[0]  * m.a[11] * m.a[14] - 
             m.a[8]  * m.a[2] * m.a[15] + 
             m.a[8]  * m.a[3] * m.a[14] + 
             m.a[12] * m.a[2] * m.a[11] - 
             m.a[12] * m.a[3] * m.a[10];

    inv.a[9] = -m.a[0]  * m.a[9] * m.a[15] + 
              m.a[0]  * m.a[11] * m.a[13] + 
              m.a[8]  * m.a[1] * m.a[15] - 
              m.a[8]  * m.a[3] * m.a[13] - 
              m.a[12] * m.a[1] * m.a[11] + 
              m.a[12] * m.a[3] * m.a[9];

    inv.a[13] = m.a[0]  * m.a[9] * m.a[14] - 
              m.a[0]  * m.a[10] * m.a[13] - 
              m.a[8]  * m.a[1] * m.a[14] + 
              m.a[8]  * m.a[2] * m.a[13] + 
              m.a[12] * m.a[1] * m.a[10] - 
              m.a[12] * m.a[2] * m.a[9];

    inv.a[2] = m.a[1]  * m.a[6] * m.a[15] - 
             m.a[1]  * m.a[7] * m.a[14] - 
             m.a[5]  * m.a[2] * m.a[15] + 
             m.a[5]  * m.a[3] * m.a[14] + 
             m.a[13] * m.a[2] * m.a[7] - 
             m.a[13] * m.a[3] * m.a[6];

    inv.a[6] = -m.a[0]  * m.a[6] * m.a[15] + 
              m.a[0]  * m.a[7] * m.a[14] + 
              m.a[4]  * m.a[2] * m.a[15] - 
              m.a[4]  * m.a[3] * m.a[14] - 
              m.a[12] * m.a[2] * m.a[7] + 
              m.a[12] * m.a[3] * m.a[6];

    inv.a[10] = m.a[0]  * m.a[5] * m.a[15] - 
              m.a[0]  * m.a[7] * m.a[13] - 
              m.a[4]  * m.a[1] * m.a[15] + 
              m.a[4]  * m.a[3] * m.a[13] + 
              m.a[12] * m.a[1] * m.a[7] - 
              m.a[12] * m.a[3] * m.a[5];

    inv.a[14] = -m.a[0]  * m.a[5] * m.a[14] + 
               m.a[0]  * m.a[6] * m.a[13] + 
               m.a[4]  * m.a[1] * m.a[14] - 
               m.a[4]  * m.a[2] * m.a[13] - 
               m.a[12] * m.a[1] * m.a[6] + 
               m.a[12] * m.a[2] * m.a[5];

    inv.a[3] = -m.a[1] * m.a[6] * m.a[11] + 
              m.a[1] * m.a[7] * m.a[10] + 
              m.a[5] * m.a[2] * m.a[11] - 
              m.a[5] * m.a[3] * m.a[10] - 
              m.a[9] * m.a[2] * m.a[7] + 
              m.a[9] * m.a[3] * m.a[6];

    inv.a[7] = m.a[0] * m.a[6] * m.a[11] - 
             m.a[0] * m.a[7] * m.a[10] - 
             m.a[4] * m.a[2] * m.a[11] + 
             m.a[4] * m.a[3] * m.a[10] + 
             m.a[8] * m.a[2] * m.a[7] - 
             m.a[8] * m.a[3] * m.a[6];

    inv.a[11] = -m.a[0] * m.a[5] * m.a[11] + 
               m.a[0] * m.a[7] * m.a[9] + 
               m.a[4] * m.a[1] * m.a[11] - 
               m.a[4] * m.a[3] * m.a[9] - 
               m.a[8] * m.a[1] * m.a[7] + 
               m.a[8] * m.a[3] * m.a[5];

    inv.a[15] = m.a[0] * m.a[5] * m.a[10] - 
              m.a[0] * m.a[6] * m.a[9] - 
              m.a[4] * m.a[1] * m.a[10] + 
              m.a[4] * m.a[2] * m.a[9] + 
              m.a[8] * m.a[1] * m.a[6] - 
              m.a[8] * m.a[2] * m.a[5];

    det = m.a[0] * inv.a[0] + m.a[1] * inv.a[4] + m.a[2] * inv.a[8] + m.a[3] * inv.a[12];

    if (det == 0.f)
        return m4_zero();

    det = 1.0 / det;

	m4 r;

    for (i = 0; i < 16; i++)
        r.a[i] = inv.a[i] * det;

	return r;
}

// TODO
inline m4 m4_mul(m4 m1, m4 m0)
{
	m4 r;
	r.v[0][0] = m0.v[0][0] * m1.v[0][0] + m0.v[0][1] * m1.v[1][0] + m0.v[0][2] * m1.v[2][0] + m0.v[0][3] * m1.v[3][0];
	r.v[0][1] = m0.v[0][0] * m1.v[0][1] + m0.v[0][1] * m1.v[1][1] + m0.v[0][2] * m1.v[2][1] + m0.v[0][3] * m1.v[3][1];
	r.v[0][2] = m0.v[0][0] * m1.v[0][2] + m0.v[0][1] * m1.v[1][2] + m0.v[0][2] * m1.v[2][2] + m0.v[0][3] * m1.v[3][2];
	r.v[0][3] = m0.v[0][0] * m1.v[0][3] + m0.v[0][1] * m1.v[1][3] + m0.v[0][2] * m1.v[2][3] + m0.v[0][3] * m1.v[3][3];

	r.v[1][0] = m0.v[1][0] * m1.v[0][0] + m0.v[1][1] * m1.v[1][0] + m0.v[1][2] * m1.v[2][0] + m0.v[1][3] * m1.v[3][0];
	r.v[1][1] = m0.v[1][0] * m1.v[0][1] + m0.v[1][1] * m1.v[1][1] + m0.v[1][2] * m1.v[2][1] + m0.v[1][3] * m1.v[3][1];
	r.v[1][2] = m0.v[1][0] * m1.v[0][2] + m0.v[1][1] * m1.v[1][2] + m0.v[1][2] * m1.v[2][2] + m0.v[1][3] * m1.v[3][2];
	r.v[1][3] = m0.v[1][0] * m1.v[0][3] + m0.v[1][1] * m1.v[1][3] + m0.v[1][2] * m1.v[2][3] + m0.v[1][3] * m1.v[3][3];

	r.v[2][0] = m0.v[2][0] * m1.v[0][0] + m0.v[2][1] * m1.v[1][0] + m0.v[2][2] * m1.v[2][0] + m0.v[2][3] * m1.v[3][0];
	r.v[2][1] = m0.v[2][0] * m1.v[0][1] + m0.v[2][1] * m1.v[1][1] + m0.v[2][2] * m1.v[2][1] + m0.v[2][3] * m1.v[3][1];
	r.v[2][2] = m0.v[2][0] * m1.v[0][2] + m0.v[2][1] * m1.v[1][2] + m0.v[2][2] * m1.v[2][2] + m0.v[2][3] * m1.v[3][2];
	r.v[2][3] = m0.v[2][0] * m1.v[0][3] + m0.v[2][1] * m1.v[1][3] + m0.v[2][2] * m1.v[2][3] + m0.v[2][3] * m1.v[3][3];

	r.v[3][0] = m0.v[3][0] * m1.v[0][0] + m0.v[3][1] * m1.v[1][0] + m0.v[3][2] * m1.v[2][0] + m0.v[3][3] * m1.v[3][0];
	r.v[3][1] = m0.v[3][0] * m1.v[0][1] + m0.v[3][1] * m1.v[1][1] + m0.v[3][2] * m1.v[2][1] + m0.v[3][3] * m1.v[3][1];
	r.v[3][2] = m0.v[3][0] * m1.v[0][2] + m0.v[3][1] * m1.v[1][2] + m0.v[3][2] * m1.v[2][2] + m0.v[3][3] * m1.v[3][2];
	r.v[3][3] = m0.v[3][0] * m1.v[0][3] + m0.v[3][1] * m1.v[1][3] + m0.v[3][2] * m1.v[2][3] + m0.v[3][3] * m1.v[3][3];
	
	return r;
}

inline void m4_set_translation(m4* m, f32 x, f32 y, f32 z)
{
	m->v[0][3] = x;
	m->v[1][3] = y;
	m->v[2][3] = z;
}

inline m4 m4_translate(f32 x, f32 y, f32 z)
{
	m4 m = m4_identity();
	m4_set_translation(&m, x, y, z);
	return m;
}

inline m4 m4_translate_v3(v3 position)
{
	return m4_translate(position.x, position.y, position.z);
}

inline m4 m4_scale(f32 x, f32 y, f32 z)
{
	m4 m = m4_zero();
	m.v[0][0] = x;
	m.v[1][1] = y;
	m.v[2][2] = z;
	m.v[3][3] = 1.f;
	return m;
}

inline m4 m4_scale_v3(v3 scale)
{
	return m4_scale(scale.x, scale.y, scale.z);
}

inline m4 m4_scale_f32(f32 scale)
{
	return m4_scale(scale, scale, scale);
}

inline m4 m4_rotate_roll(f32 roll)
{
	f32 s = math_sin(roll);
	f32 c = math_cos(roll);
	
	m4 m = m4_identity();
	m.v[0][0] = c;
	m.v[0][1] = -s;
	m.v[1][0] = s;
	m.v[1][1] = c;
	return m;
}

inline m4 m4_rotate_pitch(f32 pitch)
{
	f32 s = math_sin(pitch);
	f32 c = math_cos(pitch);
	
	m4 m = m4_identity();
	m.v[1][1] = c;
	m.v[1][2] = -s;
	m.v[2][1] = s;
	m.v[2][2] = c;
	
	return m;
}

inline m4 m4_rotate_yaw(f32 yaw)
{
	f32 s = math_sin(yaw);
	f32 c = math_cos(yaw);
	
	m4 m = m4_identity();
	m.v[0][0] = c;
	m.v[0][2] = s;
	m.v[2][0] = -s;
	m.v[2][2] = c;
	
	return m;
}

inline m4 m4_rotate_euler(f32 roll, f32 pitch, f32 yaw)
{
	m4 roll_matrix = m4_rotate_roll(roll);
	m4 pitch_matrix = m4_rotate_pitch(pitch);
	m4 yaw_matrix = m4_rotate_yaw(yaw);
	
	return m4_mul(m4_mul(roll_matrix, pitch_matrix), yaw_matrix);
}

inline m4 m4_rotate_x(f32 x)
{
	return m4_rotate_pitch(x);
}

inline m4 m4_rotate_y(f32 y)
{
	return m4_rotate_yaw(y);
}

inline m4 m4_rotate_z(f32 z)
{
	return m4_rotate_roll(z);
}

inline m4 m4_rotate_xyz(f32 x, f32 y, f32 z)
{
	return m4_rotate_euler(z, x, y);
}

// This code is stolen from ThinMatrix skeletal animation tutorial.
// I don't know wtf is going on here, but I should
// TODO: Understand this
inline m4 m4_rotate_quaternion(v4 quaternion)
{
	m4 matrix;
	const f32 xy = quaternion.x * quaternion.y;
	const f32 xz = quaternion.x * quaternion.z;
	const f32 xw = quaternion.x * quaternion.w;
	const f32 yz = quaternion.y * quaternion.z;
	const f32 yw = quaternion.y * quaternion.w;
	const f32 zw = quaternion.z * quaternion.w;
	const f32 xSquared = quaternion.x * quaternion.x;
	const f32 ySquared = quaternion.y * quaternion.y;
	const f32 zSquared = quaternion.z * quaternion.z;
	matrix.m00 = 1.f - 2.f * (ySquared + zSquared);
	matrix.m01 = 2.f * (xy - zw);
	matrix.m02 = 2.f * (xz + yw);
	matrix.m03 = 0.f;
	matrix.m10 = 2.f * (xy + zw);
	matrix.m11 = 1.f - 2.f * (xSquared + zSquared);
	matrix.m12 = 2.f * (yz - xw);
	matrix.m13 = 0.f;
	matrix.m20 = 2.f * (xz - yw);
	matrix.m21 = 2.f * (yz + xw);
	matrix.m22 = 1.f - 2.f * (xSquared + ySquared);
	matrix.m23 = 0.f;
	matrix.m30 = 0.f;
	matrix.m31 = 0.f;
	matrix.m32 = 0.f;
	matrix.m33 = 1.f;
	return matrix;
}

inline m4 m4_projection_orthographic(f32 right, f32 left, f32 top, f32 bottom, f32 near, f32 far)
{
	m4 m = m4_zero();
	m.v[0][0] = 2.f / (right - left);
	m.v[0][3] = - ((right + left) / (right - left));
	m.v[1][1] = 2.f / (top - bottom);
	m.v[1][3] = - ((top + bottom) / (top - bottom));
	m.v[2][2] = -2.f / (far - near);
	m.v[2][3] = - ((top + bottom) / (top - bottom));
	m.v[3][3] = 1.f;
	return m;
}

// Left handed perspective matrix
inline m4 m4_projection_perspective_lh(f32 aspect, f32 fov, f32 near, f32 far)
{
	m4 m = m4_zero();

	m.v[0][0] = 1.f / (aspect * tan(fov * 0.5f));
	m.v[1][1] = 1.f / tan(fov * 0.5f);
	m.v[2][2] = (far + near) / (far - near);
	m.v[2][3] = - (2.f * far * near) / (far - near);
	m.v[3][2] = 1.f;
	
	return m;
}

inline v3 m4_decompose_position(m4 m)
{
	v3 position;
	position.x = m.v[0][3];
	position.y = m.v[1][3];
	position.z = m.v[2][3];
	return position;
}

// This code is stolen from ThinMatrix skeletal animation tutorial.
// I don't know wtf is going on here, but I should
// TODO: Understand this
inline v4 m4_decompose_rotation(m4 matrix)
{
	v4 q;
	f32 diagonal = matrix.m00 + matrix.m11 + matrix.m22;
	if (diagonal > 0) {
		f32 w4 = math_sqrt(diagonal + 1.f) * 2.f;
		q.w = w4 / 4.f;
		q.x = (matrix.m21 - matrix.m12) / w4;
		q.y = (matrix.m02 - matrix.m20) / w4;
		q.z = (matrix.m10 - matrix.m01) / w4;
	}
	else if ((matrix.m00 > matrix.m11) && (matrix.m00 > matrix.m22)) {
		f32 x4 = math_sqrt(1.f + matrix.m00 - matrix.m11 - matrix.m22) * 2.f;
		q.w = (matrix.m21 - matrix.m12) / x4;
		q.x = x4 / 4.f;
		q.y = (matrix.m01 + matrix.m10) / x4;
		q.z = (matrix.m02 + matrix.m20) / x4;
	}
	else if (matrix.m11 > matrix.m22) {
		f32 y4 = math_sqrt(1.f + matrix.m11 - matrix.m00 - matrix.m22) * 2.f;
		q.w = (matrix.m02 - matrix.m20) / y4;
		q.x = (matrix.m01 + matrix.m10) / y4;
		q.y = y4 / 4.f;
		q.z = (matrix.m12 + matrix.m21) / y4;
	}
	else {
		f32 z4 = math_sqrt(1.f + matrix.m22 - matrix.m00 - matrix.m11) * 2.f;
		q.w = (matrix.m10 - matrix.m01) / z4;
		q.x = (matrix.m02 + matrix.m20) / z4;
		q.y = (matrix.m12 + matrix.m21) / z4;
		q.z = z4 / 4.f;
	}
	return q;
}

// Quaternion

// This code is stolen from ThinMatrix skeletal animation tutorial.
// I don't know wtf is going on here, but I should
// TODO: Understand this
inline v4 quaternion_interpolate(v4 a, v4 b, f32 n)
{
	v4 q = v4_set(0.f, 0.f, 0.f, 1.f);
	f32 dot = a.w * b.w + a.x * b.x + a.y * b.y + a.z * b.z;
	f32 blendI = 1.f - n;
	if (dot < 0) {
		q.w = blendI * a.w + n * -b.w;
		q.x = blendI * a.x + n * -b.x;
		q.y = blendI * a.y + n * -b.y;
		q.z = blendI * a.z + n * -b.z;
	}
	else {
		q.w = blendI * a.w + n * b.w;
		q.x = blendI * a.x + n * b.x;
		q.y = blendI * a.y + n * b.y;
		q.z = blendI * a.z + n * b.z;
	}
	q = v4_normalize(q);
	return q;
}

inline v4 quaternion_from_euler(f32 roll, f32 pitch, f32 yaw)
{
	// TODO: Optimize xd
	m4 r = m4_rotate_euler(roll, pitch, yaw);
	return m4_decompose_rotation(r);
}

// Ray

inline Ray ray_mouse_picking_perspective(v2 mouse_position, v3 camera_position, m4 inverse_view_matrix, m4 inverse_projection_matrix)
{
	Ray ray;
	
	// Screen to clip space
	v2 position = v2_mul_scalar(mouse_position, 2.f);

	v4 mouse_world = v4_set(position.x, position.y, 1.f, 1.f);
	mouse_world = v4_transform(mouse_world, inverse_projection_matrix);
	mouse_world.z = 1.f;
	mouse_world.w = 0.f;
	mouse_world = v4_transform(mouse_world, inverse_view_matrix);
			
	ray.origin = camera_position;
	ray.direction = v3_normalize(v4_to_v3(mouse_world));

	return ray;
}

inline Ray ray_transform(Ray ray, m4 matrix)
{
	v4 origin = v4_transform(v3_to_v4(ray.origin, 1.f), matrix);
	v4 direction = v4_transform(v3_to_v4(ray.direction, 0.f), matrix);

	ray.origin = v4_to_v3(origin);
	ray.direction = v4_to_v3(direction);
	return ray;
}

inline b8 ray_intersect_triangle(Ray ray, const v3 p0, const v3 p1, const v3 p2, v3* out)
{
	const f32 EPSILON = 0.0000001f;
	v3 edge1, edge2, h, s, q;
	f32 a, f, u, v;
	edge1 = v3_sub(p1, p0);
	edge2 = v3_sub(p2, p0);
	h = v3_cross(ray.direction, edge2);
	a = v3_dot(edge1, h);
	if (a > -EPSILON && a < EPSILON)
		return FALSE;    // This ray is parallel to this triangle.
	f = 1.f / a;
	s = v3_sub(ray.origin, p0);
	u = f * v3_dot(s, h);
	if (u < 0.0 || u > 1.0)
		return FALSE;
	q = v3_cross(s, edge1);
	v = f * v3_dot(ray.direction, q);
	if (v < 0.0 || u + v > 1.0)
		return FALSE;
	// At this stage we can compute t to find out where the intersection point is on the line.
	f32 t = f * v3_dot(edge2, q);
	if (t > EPSILON) // ray intersection
	{
		*out= v3_mul_scalar(ray.direction, t);
		*out= v3_add(ray.origin, *out);
		return TRUE;
	}
	else // This means that there is a line intersection but not a ray intersection.
		return TRUE;
}

// From: https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-box-intersection
// TODO: Optimize
inline b8 ray_intersect_aabb(Ray ray, const v3 min, const v3 max, f32* dist)
{
	float tmin = (min.x - ray.origin.x) / ray.direction.x;
	float tmax = (max.x - ray.origin.x) / ray.direction.x;

	if (tmin > tmax) {
		f32 aux = tmin;
		tmin = tmax;
		tmax = aux;
	}

	float tymin = (min.y - ray.origin.y) / ray.direction.y;
	float tymax = (max.y - ray.origin.y) / ray.direction.y;

	if (tymin > tymax) {
		f32 aux = tymin;
		tymin = tymax;
		tymax = aux;
	}

	if ((tmin > tymax) || (tymin > tmax))
		return FALSE;

	if (tymin > tmin)
		tmin = tymin;

	if (tymax < tmax)
		tmax = tymax;

	float tzmin = (min.z - ray.origin.z) / ray.direction.z;
	float tzmax = (max.z - ray.origin.z) / ray.direction.z;

	if (tzmin > tzmax) {
		f32 aux = tzmin;
		tzmin = tzmax;
		tzmax = aux;
	}

	if ((tmin > tzmax) || (tzmin > tmax))
		return FALSE;

	if (tzmin > tmin)
		tmin = tzmin;

	if (tzmax < tmax)
		tmax = tzmax;

	*dist = tmin;

	return TRUE;
}

// View Frustum

#define FRUSTUM_NEAR 0
#define FRUSTUM_LEFT 1
#define FRUSTUM_RIGHT 2
#define FRUSTUM_DOWN 3
#define FRUSTUM_UP 4
#define FRUSTUM_FAR 5

typedef struct {

	// Plane Order: NEAR, LEFT, RIGHT, DOWN, UP, FAR

	v4 planes[6]; // xyz ->Normal, w -> distance to origin
} Frustum;

inline Frustum frustum_calculate(m4 m)
{
	Frustum f;
	// TODO: wtf is going on

	f.planes[FRUSTUM_NEAR].x = m.v[0][3] + m.v[0][2];
	f.planes[FRUSTUM_NEAR].y = m.v[1][3] + m.v[1][2];
	f.planes[FRUSTUM_NEAR].z = m.v[2][3] + m.v[2][2];
	f.planes[FRUSTUM_NEAR].w = m.v[3][3] + m.v[3][2];

	f.planes[FRUSTUM_LEFT].x = m.v[0][3] + m.v[0][0];
	f.planes[FRUSTUM_LEFT].y = m.v[1][3] + m.v[1][0];
	f.planes[FRUSTUM_LEFT].z = m.v[2][3] + m.v[2][0];
	f.planes[FRUSTUM_LEFT].w = m.v[3][3] + m.v[3][0];

	f.planes[FRUSTUM_RIGHT].x = m.v[0][3] - m.v[0][0];
	f.planes[FRUSTUM_RIGHT].y = m.v[1][3] - m.v[1][0];
	f.planes[FRUSTUM_RIGHT].z = m.v[2][3] - m.v[2][0];
	f.planes[FRUSTUM_RIGHT].w = m.v[3][3] - m.v[3][0];

	f.planes[FRUSTUM_DOWN].x = m.v[0][3] + m.v[0][1];
	f.planes[FRUSTUM_DOWN].y = m.v[1][3] + m.v[1][1];
	f.planes[FRUSTUM_DOWN].z = m.v[2][3] + m.v[2][1];
	f.planes[FRUSTUM_DOWN].w = m.v[3][3] + m.v[3][1];

	f.planes[FRUSTUM_UP].x = m.v[0][3] - m.v[0][1];
	f.planes[FRUSTUM_UP].y = m.v[1][3] - m.v[1][1];
	f.planes[FRUSTUM_UP].z = m.v[2][3] - m.v[2][1];
	f.planes[FRUSTUM_UP].w = m.v[3][3] - m.v[3][1];

	f.planes[FRUSTUM_FAR].x = m.v[0][3] - m.v[0][2];
	f.planes[FRUSTUM_FAR].y = m.v[1][3] - m.v[1][2];
	f.planes[FRUSTUM_FAR].z = m.v[2][3] - m.v[2][2];
	f.planes[FRUSTUM_FAR].w = m.v[3][3] - m.v[3][2];

	foreach(i, 6) {

		v3 normal = v4_to_v3(f.planes[i]);
		normal = v3_normalize(normal);

		f.planes[i].x = normal.x;
		f.planes[i].y = normal.y;
		f.planes[i].z = normal.z;
	}

	return f;
}

inline b8 frustum_intersect_sphere(const Frustum* frustum, v3 to_center, f32 radius)
{
	foreach(i, 6) {

		v3 normal = v4_to_v3(frustum->planes[i]);
		f32 distance = frustum->planes[i].w;

		f32 e = v3_dot(to_center, normal);

		e += distance + radius;
		if (e <= 0.f)
			return FALSE;
	}

	return TRUE;
}

// Color

inline Color color_rgba(u8 r, u8 g, u8 b, u8 a)
{
	Color c;
	c.r = r;
	c.g = g;
	c.b = b;
	c.a = a;
	return c;
}

inline Color color_rgb(u8 r, u8 g, u8 b)
{
	Color c;
	c.r = r;
	c.g = g;
	c.b = b;
	c.a = 255;
	return c;
}

inline Color color_rgba_f32(f32 r, f32 g, f32 b, f32 a)
{
	return color_rgba((u8)(r * 255.f), (u8)(g * 255.f), (u8)(b * 255.f), (u8)(a * 255.f));
}

inline Color color_rgb_f32(f32 r, f32 g, f32 b)
{
	return color_rgb((u8)(r * 255.f), (u8)(g * 255.f), (u8)(b * 255.f));
}

inline b8 color_equals(Color c0, Color c1)
{
	return c0.r == c1.r &&
		c0.g == c1.g &&
		c0.b == c1.b &&
		c0.a == c1.a;
}

inline v3 color_to_v3(Color c)
{
	f32 mult = (1.f / 255.f);
	return v3_set((f32)c.r * mult, (f32)c.g * mult, (f32)c.b * mult);
}
	
inline v4 color_to_v4(Color c)
{
	f32 mult = (1.f / 255.f);
	return v4_set((f32)c.r * mult, (f32)c.g * mult, (f32)c.b * mult, (f32)c.a * mult);
}

inline Color color_blend(Color c0, Color c1)
{
	Color c;
	c.r = (c0.r / 2u) + (c1.r / 2u);
	c.g = (c0.g / 2u) + (c1.g / 2u);
	c.b = (c0.b / 2u) + (c1.b / 2u);
	c.a = (c0.a / 2u) + (c1.a / 2u);
	return c;
}

inline Color color_interpolate(Color c0, Color c1, f32 n)
{
	Color c;
	c.r = (u8)((f32)(c0.r) * (1.f - n)) + (u8)((f32)(c1.r) * n);
	c.g = (u8)((f32)(c0.g) * (1.f - n)) + (u8)((f32)(c1.g) * n);
	c.b = (u8)((f32)(c0.b) * (1.f - n)) + (u8)((f32)(c1.b) * n);
	c.a = (u8)((f32)(c0.a) * (1.f - n)) + (u8)((f32)(c1.a) * n);
	return c;
}

inline Color color_transparent() { return color_rgba(0u, 0u, 0u, 0u); }
inline Color color_red()         { return color_rgb(255u, 0u, 0u); }
inline Color color_green()       { return color_rgb(0u, 128u, 0u); }
inline Color color_blue()        { return color_rgb(0u, 0u, 255u); }
inline Color color_orange()      { return color_rgb(255u, 69u, 0u); }
inline Color color_black()       { return color_rgb(0u, 0u, 0u); }
inline Color color_gray(u8 i)    { return color_rgb(i, i, i); }
inline Color color_white()       { return color_rgb(255u, 255u, 255u); }

// Random

// return value from 0u to (u32_max / 2u)
inline u32 math_random_u32(u32 seed)
{
	seed = (seed << 13) ^ seed;
	return ((seed * (seed * seed * 15731u * 789221u) + 1376312589u) & 0x7fffffffu);
}

inline f32 math_random_f32(u32 seed)
{
	return (f32)math_random_u32(seed) * (1.f / (f32)(u32_max / 2u));
}
inline f32 math_random_f32_max(u32 seed, f32 max)
{
	return math_random_f32(seed) * max;
}
inline f32 math_random_f32_min_max(u32 seed, f32 min, f32 max)
{
	return min + math_random_f32(seed) * (max - min);
}

inline u32 math_random_u32_max(u32 seed, u32 max)
{
	if (max > 8u)
		return math_random_u32(seed) % max;
	else
		return (u32)math_random_f32_max(++seed, (f32)max);
}
inline u32 math_random_u32_min_max(u32 seed, u32 min, u32 max)
{
	if (max - min > 8u)
		return min + (math_random_u32(seed) % (max - min));
	else
		return (u32)math_random_f32_min_max(++seed, (f32)min, (f32)max);
}

inline f32 math_perlin_noise(u32 seed, f32 n)
{
	n += math_random_f32(seed * 0x3294124u) * 10000.f - 5000.f;

	i32 offset = (n < 0.f) ? (i32)(n - 1.f) : (i32)n;

	f32 p0 = (f32)offset;
	f32 h0 = math_random_f32(seed + offset);

	if (n < p0) {

		--offset;
		f32 p1 = (f32)offset;

		f32 pos = (n - p1) / (p0 - p1);
		pos = math_cos(PI - pos * PI) * 0.5f + 0.5f;

		f32 h1 = math_random_f32(seed + offset);

		return h1 + pos * (h0 - h1);
	}
	else {

		++offset;
		f32 p1 = (f32)offset;

		f32 pos = (n - p0) / (p1 - p0);
		pos = math_cos(PI - pos * -PI) * 0.5f + 0.5f;

		f32 h1 = math_random_f32(seed + offset);

		return h0 + pos * (h1 - h0);
	}
}

inline f32 _perlin_noise_2D_peak_value(u32 seed, i32 p, f32 x)
{
	seed += p;
	f32 desp = math_random_f32(seed);
	return math_perlin_noise(seed, x + desp);
}

inline f32 math_perlin_noise2D(u32 seed, f32 x, f32 y)
{
	i32 p0 = (i32)y;
	if (y < 0.f) --p0;

	i32 p1 = p0 + 1;

	f32 n0 = _perlin_noise_2D_peak_value(seed, p0, x);
	f32 n1 = _perlin_noise_2D_peak_value(seed, p1, x);

	f32 inter = y - (f32)p0 / ((f32)p1 - (f32)p0);

	inter = (math_cos(inter * PI) - 1.f) * -0.5f;

	return n0 * (1.f - inter) + n1 * inter;
}

inline f32 _perlin_noise_3D_peak_value(u32 seed, i32 p, f32 x, f32 y)
{
	seed += p;
	f32 desp0 = math_random_f32(seed);
	f32 desp1 = math_random_f32(seed + 0x25462u);
	return math_perlin_noise2D(seed, x + desp0, y + desp1);
}

inline f32 math_perlin_noise3D(u32 seed, f32 x, f32 y, f32 z)
{
	i32 p0 = (i32)z;
	if (z < 0.f) --p0;

	i32 p1 = p0 + 1;

	f32 n0 = _perlin_noise_3D_peak_value(seed, p0, x, y);
	f32 n1 = _perlin_noise_3D_peak_value(seed, p1, x, y);

	f32 inter = z - (f32)p0 / ((f32)p1 - (f32)p0);

	inter = (math_cos(inter * PI) - 1.f) * -0.5f;

	return n0 * (1.f - inter) + n1 * inter;
}

inline f32 math_ridged_noise(f32 n, f32 e)
{
	n = 2.f * (0.5f - fabs(0.5f - n));
	n = pow(n, e);
	return n;
}

/*
    // Matrix

	inline XMMATRIX mat_view_from_quaternion(v3_f32 position, v4_f32 quaternion)
	{
		XMMATRIX m = XMMatrixRotationQuaternion(vec4_to_dx(quaternion)) * XMMatrixTranslation(position.x, position.y, position.z);
		
		m = XMMatrixInverse(NULL, m);
		return m;
	}

	inline XMMATRIX mat_view_from_direction(v3_f32 position, v3_f32 direction, v3_f32 up)
	{
		v3_f32 cam_right = vec3_normalize(vec3_cross(direction, up));
		v3_f32 cam_up = vec3_cross(cam_right, direction);

		XMFLOAT4X4 mat;
		mat(1, 1) = cam_right.x;
		mat(2, 1) = cam_right.y;
		mat(3, 1) = cam_right.z;
		mat(4, 1) = 0.f;
		mat(1, 2) = cam_up.x;
		mat(2, 2) = cam_up.y;
		mat(3, 2) = cam_up.z;
		mat(4, 2) = 0.f;
		mat(1, 3) = -direction.x;
		mat(2, 3) = -direction.y;
		mat(3, 3) = -direction.z;
		mat(4, 3) = 0.f;
		mat(1, 4) = position.x;
		mat(2, 4) = position.y;
		mat(3, 4) = position.z;
		mat(4, 4) = 1.f;

		// TODO: position

		return XMMatrixInverse(NULL, XMMatrixTranspose(XMLoadFloat4x4(&mat)));
	}

	// Quaternion

	// TODO: WTF is going on
	inline v3_f32 quaternion_to_euler_angles(v4_f32 quat)
	{
		v3_f32 euler;

		// roll (x-axis rotation)

		XMFLOAT4 q;
		q.x = quat.x;
		q.y = quat.y;
		q.z = quat.z;
		q.w = quat.w;
		float sinr_cosp = 2.f * (q.w * q.x + q.y * q.z);
		float cosr_cosp = 1.f - 2.f * (q.x * q.x + q.y * q.y);
		euler.x = atan2f(sinr_cosp, cosr_cosp);

		// pitch (y-axis rotation)
		float sinp = 2.f * (q.w * q.y - q.z * q.x);
		if (abs(sinp) >= 1.f)
			euler.y = copysignf(PI / 2.f, sinp); // use 90 degrees if out of range
		else
			euler.y = asinf(sinp);

		// yaw (z-axis rotation)
		float siny_cosp = 2 * (q.w * q.z + q.x * q.y);
		float cosy_cosp = 1 - 2 * (q.y * q.y + q.z * q.z);
		euler.z = atan2f(siny_cosp, cosy_cosp);

		if (euler.x < 0.f) {
			euler.x = 2.f * PI + euler.x;
		}
		if (euler.y < 0.f) {
			euler.y = 2.f * PI + euler.y;
		}
		if (euler.z < 0.f) {
			euler.z = 2.f * PI + euler.z;
		}

		return euler;
    }

	// TODO: WTF is going on 2
	inline v4_f32 quaternion_from_euler_angles(v3_f32 euler)
	{
		float cy = cosf(euler.z * 0.5f);
		float sy = sinf(euler.z * 0.5f);
		float cp = cosf(euler.y * 0.5f);
		float sp = sinf(euler.y * 0.5f);
		float cr = cosf(euler.x * 0.5f);
		float sr = sinf(euler.x * 0.5f);

		v4_f32 q;
		q.w = cr * cp * cy + sr * sp * sy;
		q.x = sr * cp * cy - cr * sp * sy;
		q.y = cr * sp * cy + sr * cp * sy;
		q.z = cr * cp * sy - sr * sp * cy;

		return q;
	}

    // Intersection

    struct Ray {
		v3_f32 origin;
		v3_f32 direction;
    };
	
   
*/

// Hash functions

inline u64 hash_combine(u64 hash, u64 value)
{
	value *= 942341895;
	value >>= 8;
	value *= 858432321;
	value >>= 8;
	value *= 239823573;
		
	hash ^= value + 0x9e3779b9 + (hash << 6) + (hash >> 2);
	return hash;
}

inline u64 hash_memory(u64 hash, const u8* data, u32 size)
{
	const u8* it = data;
	const u8* end = data + size;

	while (it != end) {

		u64 value = 0u;

		u32 count = SV_MIN(it + 4, end) - it;

		foreach(i, count) {
			
			u8 byte = it[i];

			value |= byte << (i * 8);
		}

		it += count;

		hash = hash_combine(hash, value);
	}

	return hash;
}

inline u64 hash_string(const char* str)
{
	u32 size = (u32)string_size(str);
	return hash_memory(0, (const u8*)str, size);
}
