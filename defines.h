#pragma once

#ifndef SV_GRAPHICS
#define SV_GRAPHICS 0
#endif

#include "stdlib.h"
#include "stdio.h"
#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

// types
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t  i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef float	f32;
typedef double	f64;

typedef char b8;
#define TRUE 1
#define FALSE 0

// limits

#define FILE_PATH_SIZE 260
#define FILE_NAME_SIZE 51

#define u8_max 0xFF
#define u16_max 0xFFFF
#define u32_max 0xFFFFFFFF
#define u64_max 0xFFFFFFFFFFFFFFFF

/*constexpr i8	i8_min		= std::numeric_limits<i8>::min();
constexpr i16	i16_min		= std::numeric_limits<i16>::min();
constexpr i32	i32_min		= std::numeric_limits<i32>::min();
constexpr i64	i64_min		= std::numeric_limits<i64>::min();

constexpr i8	i8_max = std::numeric_limits<i8>::max();
constexpr i16	i16_max = std::numeric_limits<i16>::max();
constexpr i32	i32_max = std::numeric_limits<i32>::max();
constexpr i64	i64_max = std::numeric_limits<i64>::max();

constexpr f32	f32_min = std::numeric_limits<f32>::min();
constexpr f64	f64_min = std::numeric_limits<f64>::min();

constexpr f32	f32_max = std::numeric_limits<f32>::max();
constexpr f64	f64_max = std::numeric_limits<f64>::max();*/

// Misc

#define SV_BIT(x) (1ULL << x) 
#define foreach(_it, _end) for (u32 _it = 0u; _it < (u32)(_end); ++_it)
#define SV_MIN(a, b) ((a < b) ? a : b)
#define SV_MAX(a, b) ((a > b) ? a : b)
#define SV_ARRAY_SIZE(x) (sizeof(x) / sizeof(*x))
#define SV_CHECK(x) if (!(x)) return FALSE
#define SV_ZERO(x) memory_zero(&(x), sizeof(x))
#define SV_BUFFER_WRITE(it, src) do { memory_copy((it), &(src), sizeof(src)); it += sizeof(src); } while(0)
#define SV_BUFFER_READ(it, dst) do { memory_copy(&(dst), (it), sizeof(dst)); it += sizeof(dst); } while(0)

// TODO

#if SV_SLOW

void throw_assertion(const char* title, u32 line, const char* file);
	
#define assert(x) do { if (!(x)) { throw_assertion(#x, __LINE__, __FILE__); } } while(0)
#define assert_title(x, title) do { if (!(x)) { throw_assertion(title, __LINE__, __FILE__); } } while(0)

#define SV_LOG_INFO(x, ...) print("[INFO]" x, __VA_ARGS__)
#define SV_LOG_WARNING(x, ...) print("[WARNING]" x, __VA_ARGS__)
#define SV_LOG_ERROR(x, ...) print("[ERROR]" x, __VA_ARGS__)

#else

#define assert(x) {}
#define assert_title(x, title) {}

#define SV_LOG_INFO(x, ...) {}
#define SV_LOG_WARNING(x, ...) {}
#define SV_LOG_ERROR(x, ...) {}

#endif

typedef struct {
	f32 delta_time;
	f32 time_step;
	u32 frame_count;
} CoreData;

extern CoreData core;

typedef struct {
		
	f32 x;
	f32 y;
	
} v2;

typedef struct {

	f32 x;
	f32 y;
	f32 z;

} v3;

typedef struct {

	f32 x;
	f32 y;
	f32 z;
	f32 w;

} v4;

typedef struct {
		
	u32 x;
	u32 y;
	
} v2_u32;

typedef struct {

	u32 x;
	u32 y;
	u32 z;

} v3_u32;

typedef struct {

	u32 x;
	u32 y;
	u32 z;
	u32 w;

} v4_u32;

typedef struct {
		
	i32 x;
	i32 y;
	
} v2_i32;

typedef struct {

	i32 x;
	i32 y;
	i32 z;

} v3_i32;

typedef struct {

	i32 x;
	i32 y;
	i32 z;
	i32 w;

} v4_i32;

typedef struct {
	union {
		f32 v[4][4];
		f32 a[16];
	};
} Mat4;	

typedef struct {
	u8 r, g, b, a;
} Color;

typedef struct {
	v3 origin;
	v3 direction;
} Ray;
	
#ifdef __cplusplus
}
#endif
