#pragma once

#ifndef SV_GRAPHICS
#define SV_GRAPHICS 0
#endif

#define SV_ARCH_32 (sizeof(void*) == 4)
#define SV_ARCH_64 (sizeof(void*) == 8)

#include "stdlib.h"
#include "stdio.h"
#include "string.h"
#include "stdint.h"

#define assert_static(x)

// types
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t  i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef u16     f16;
typedef float	f32;
typedef double	f64;

typedef u8  b8;
typedef u16 b16;
typedef u32 b32;
typedef u64 b64;
#define TRUE 1
#define FALSE 0

assert_static(sizeof(u8) == 1);
assert_static(sizeof(u16) == 2);
assert_static(sizeof(u32) == 4);
assert_static(sizeof(u64) == 8);

assert_static(sizeof(i8) == 1);
assert_static(sizeof(i16) == 2);
assert_static(sizeof(i32) == 4);
assert_static(sizeof(i64) == 8);

assert_static(sizeof(f16) == 2);
assert_static(sizeof(f32) == 4);
assert_static(sizeof(f64) == 8);

assert_static(sizeof(size_t) == sizeof(void*));

// limits

#define FILE_PATH_SIZE 260
#define FILE_NAME_SIZE 51
#define NAME_SIZE 31

#define u8_max 0xFF
#define u16_max 0xFFFF
#define u32_max 0xFFFFFFFF
#define u64_max 0xFFFFFFFFFFFFFFFF

#define i16_min -32768
#define i32_min -2147483648

#define i16_max 32767
#define i32_max 2147483647

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
#define SV_ABS(x) (((x) < 0.f) ? (-(x)) : (x))
#define SV_CLAMP(a, min, max) (SV_MAX(SV_MIN(a, max), min))
#define SV_POW2(x) ((x) * (x))
#define SV_POW3(x) ((x) * (x) * (x))
#define SV_SQRT_2 (1.414213562f)
#define SV_STRING(x) #x
#define SV_FUNCTION_NAME() __FUNCTION__
#define SV_ARRAY_SIZE(x) (sizeof(x) / sizeof(*x))
#define SV_CHECK(x) if (!(x)) return FALSE
#define SV_ZERO(x) memory_zero(&(x), sizeof(x))
#define SV_BUFFER_WRITE(it, src) do { memory_copy((it), &(src), sizeof(src)); it += sizeof(src); } while(0)
#define SV_BUFFER_READ(it, dst) do { memory_copy(&(dst), (it), sizeof(dst)); it += sizeof(dst); } while(0)
#define SV_DEREF(n, def) (((n) == NULL) ? (def) : (*(n)))

#define SV_INLINE inline static

#if SV_PLATFORM_WINDOWS
#define SV_IN_PC 1
#else
#define SV_IN_PC 0
#endif

#ifdef __cplusplus

#define SV_BEGIN_C_HEADER extern "C" {
#define SV_END_C_HEADER }

#else

#define SV_BEGIN_C_HEADER
#define SV_END_C_HEADER

#endif
	
// TODO

SV_BEGIN_C_HEADER

#if SV_SLOW

void throw_assertion(const char* title, u32 line, const char* file);
	
#define assert(x) do { if (!(x)) { throw_assertion(#x, __LINE__, __FILE__); } } while(0)
#define assert_title(x, title) do { if (!(x)) { throw_assertion(title, __LINE__, __FILE__); } } while(0)

SV_INLINE b8 throw_assertion_and_return_false(const char* title, u32 line, const char* file) { throw_assertion(title, line, file); return FALSE; }
#define if_assert(x) if ((x) ? TRUE : (throw_assertion_and_return_false(#x, __LINE__, __FILE__)))

#if SV_PLATFORM_WINDOWS

typedef enum {
	PrintStyle_Info,
	PrintStyle_Warning,
	PrintStyle_Error,
} PrintStyle;

void windows_print(PrintStyle style, const char* str, ...);

#define SV_LOG_VERBOSE(x, ...) {;}
#define SV_LOG_INFO(x, ...) windows_print(PrintStyle_Info, x, __VA_ARGS__)
#define SV_LOG_WARNING(x, ...) windows_print(PrintStyle_Warning, x, __VA_ARGS__)
#define SV_LOG_ERROR(x, ...) windows_print(PrintStyle_Error, x, __VA_ARGS__)

#elif SV_PLATFORM_ANDROID

#include <android/log.h>

#define SV_LOG_VERBOSE(x, ...) __android_log_print(ANDROID_LOG_VERBOSE, "native-activity", x, ##__VA_ARGS__)
#define SV_LOG_INFO(x, ...) __android_log_print(ANDROID_LOG_INFO, "native-activity", x, ##__VA_ARGS__)
#define SV_LOG_WARNING(x, ...) __android_log_print(ANDROID_LOG_WARN, "native-activity", x, ##__VA_ARGS__)
#define SV_LOG_ERROR(x, ...) __android_log_print(ANDROID_LOG_ERROR, "native-activity", x, ##__VA_ARGS__)

#endif

#else

#define assert(x) {;}
#define assert_title(x, title) {;}
#define if_assert(x) if (x)

#define SV_LOG_VERBOSE(x, ...) {;}
#define SV_LOG_INFO(x, ...) {;}
#define SV_LOG_WARNING(x, ...) {;}
#define SV_LOG_ERROR(x, ...) {;}

#endif

typedef struct {
	f64 last_update;
	f32 delta_time;
	f32 time_step;
	u32 frame_count;
	u32 FPS;
} CoreData;

extern CoreData core;

typedef u64 Asset;

typedef struct {
		
	union {
		struct {
			f32 x;
			f32 y;
		};
		f32 v[2];
	};
	
} v2;

typedef struct {

	union {
		struct {
			f32 x;
			f32 y;
			f32 z;
		};
		f32 v[3];
	};

} v3;

typedef struct {

	union {
		struct {
			f32 x;
			f32 y;
			f32 z;
			f32 w;
		};
		f32 v[4];
	};

} v4;

typedef struct {
		
	union {
		struct {
			u32 x;
			u32 y;
		};
		u32 v[2];
	};
	
} v2_u32;

typedef struct {

	union {
		struct {
			u32 x;
			u32 y;
			u32 z;
		};
		u32 v[3];
	};

} v3_u32;

typedef struct {

	union {
		struct {
			u32 x;
			u32 y;
			u32 z;
			u32 w;
		};
		u32 v[4];
	};

} v4_u32;

typedef struct {
		
	union {
		struct {
			i32 x;
			i32 y;
		};
		i32 v[2];
	};
	
} v2_i32;

typedef struct {

	union {
		struct {
			i32 x;
			i32 y;
			i32 z;
		};
		i32 v[3];
	};

} v3_i32;

typedef struct {

	union {
		struct {
			i32 x;
			i32 y;
			i32 z;
			i32 w;
		};
		i32 v[4];
	};

} v4_i32;

typedef struct {
		
	union {
		struct {
			i16 x;
			i16 y;
		};
		i16 v[2];
	};
	
} v2_i16;

typedef struct {

	union {
		struct {
			i16 x;
			i16 y;
			i16 z;
		};
		i16 v[3];
	};

} v3_i16;

typedef struct {

	union {
		struct {
			f16 x;
			f16 y;
			f16 z;
		};
		f16 v[3];
	};

} v3_f16;

typedef struct {

	union {
		struct {
			f16 x;
			f16 y;
			f16 z;
			f16 w;
		};
		f16 v[4];
	};

} v4_f16;

typedef struct {

	union {
		struct {
			i16 x;
			i16 y;
			i16 z;
			i16 w;
		};
		i16 v[4];
	};

} v4_i16;

typedef struct {

	union {
		struct {
			u16 x;
			u16 y;
		};
		u16 v[2];
	};

} v2_u16;

typedef struct {

	union {
		struct {
			f16 x;
			f16 y;
		};
		f16 v[2];
	};

} v2_f16;

typedef struct {

	union {
		struct {
			u8 x;
			u8 y;
		};
		u8 v[2];
	};

} v2_u8;

typedef struct {

	union {
		struct {
			b8 x;
			b8 y;
		};
		b8 v[2];
	};

} v2_b8;

typedef struct {

	union {
		struct {
			b8 x;
			b8 y;
			b8 z;
		};
		b8 v[3];
	};

} v3_b8;

typedef struct {

	union {
		struct {
			b8 x;
			b8 y;
			b8 z;
			b8 w;
		};
		b8 v[4];
	};

} v4_b8;

// Row major matrix: [column][row] 
typedef struct {
	union {
		f32 v[4][4];
		f32 a[16];
		struct {
			f32 m00, m10, m20, m30;
			f32 m01, m11, m21, m31;
			f32 m02, m12, m22, m32;
			f32 m03, m13, m23, m33;
		};
	};
} m4;

typedef struct {
	u8 r, g, b, a;
} Color;

typedef struct {
	v3 origin;
	v3 direction;
} Ray;

typedef struct {
	v2 origin;
	v2 direction;
} Ray2D;
	
SV_END_C_HEADER
