#pragma once

#include "Hosebase/defines.h"

SV_BEGIN_C_HEADER

b8 load_image(const char* filepath, void** pdata, u32* width, u32* height);

b8 bin_read(u64 hash, Buffer* data, b8 system);
//b8 bin_read(u64 hash, Deserializer& deserializer, b8 system); // Begins the deserializer

b8 bin_write(u64 hash, const void* data, size_t size, b8 system);
//b8 bin_write(u64 hash, Serializer& serializer, b8 system); // Ends the serializer

// MESH LOADING

typedef struct {

	char name[NAME_SIZE];

	DynamicArray(v3) positions;
	DynamicArray(v3) normals;
	DynamicArray(v2) texcoords;

	DynamicArray(u32) indices;

	Mat4 transform_matrix;
	u32 material_index;

	b8 import;

} MeshInfo;

typedef struct {

	char name[NAME_SIZE];
	
	// Pipeline settings
	b8 transparent;
	RasterizerCullMode culling;

	// Values
	
	Color ambient_color;
	Color diffuse_color;
	Color specular_color;
	Color emissive_color;
	f32 shininess;

	// Textures
	
	char diffuse_map_path[FILE_PATH_SIZE];
	char normal_map_path[FILE_PATH_SIZE];
	char specular_map_path[FILE_PATH_SIZE];
	char emissive_map_path[FILE_PATH_SIZE];

	b8 import;
} MaterialInfo;

typedef struct {
	char folderpath[FILE_PATH_SIZE];
	DynamicArray(MeshInfo) meshes;
	DynamicArray(MaterialInfo) materials;
} ModelInfo;

b8 model_load(ModelInfo* model_info, const char* filepath);
void model_free(ModelInfo* model_info);

// Serializer

#define SERIALIZER_VERSION 0
#define SERIALIZER_ALLOCATE 1000

typedef struct {
	u8* data;
	u32 size;
	u32 capacity;
} Serializer;

inline void serializer_write(Serializer* s, const void* data, u32 size)
{
	assert(data && size);

	if (s->size + size > s->capacity) {

		u32 new_capacity = SV_MAX(s->size + size, s->capacity + SERIALIZER_ALLOCATE);
		u8* new_data = memory_allocate(new_capacity);

		if (s->size) {
			memory_copy(new_data, s->data, s->size);
			memory_free(s->data);
		}

		s->data = new_data;
		s->capacity = new_capacity;
	}

	memory_copy(s->data + s->size, data, size);
	s->size += size;
}

inline void serializer_begin(Serializer* s, u32 initial_capacity)
{
	s->capacity = SV_MAX(initial_capacity, SERIALIZER_ALLOCATE);
	s->data = memory_allocate(s->capacity);
	s->size = 0u;

	u32 version = SERIALIZER_VERSION;
	serializer_write(s, &version, sizeof(u32));
}

inline b8 serializer_end(Serializer* s, const char* filepath)
{
	if (s->size == 0)
		return FALSE;
	
	b8 res = file_write_binary(filepath, s->data, s->size, FALSE, TRUE);

	if (s->data) memory_free(s->data);

	return res;
}

inline void serialize_u8(Serializer* s, u8 n)
{
	serializer_write(s, &n, sizeof(u8));
}

inline void serialize_u16(Serializer* s, u16 n)
{
	serializer_write(s, &n, sizeof(u16));
}

inline void serialize_u32(Serializer* s, u32 n)
{
	serializer_write(s, &n, sizeof(u32));
}

inline void serialize_u64(Serializer* s, u64 n)
{
	serializer_write(s, &n, sizeof(u64));
}

inline void serialize_size_t(Serializer* s, size_t n)
{
	u64 n0 = (u64)n;
	serializer_write(s, &n0, sizeof(u64));
}

inline void serialize_i8(Serializer* s, i8 n)
{
	serializer_write(s, &n, sizeof(i8));
}

inline void serialize_i16(Serializer* s, i16 n)
{
	serializer_write(s, &n, sizeof(i16));
}

inline void serialize_i32(Serializer* s, i32 n)
{
	serializer_write(s, &n, sizeof(i32));
}

inline void serialize_i64(Serializer* s, i64 n)
{
	serializer_write(s, &n, sizeof(i64));
}

inline void serialize_f32(Serializer* s, f32 n)
{
	serializer_write(s, &n, sizeof(f32));
}

inline void serialize_f64(Serializer* s, f64 n)
{
	serializer_write(s, &n, sizeof(f64));
}

inline void serialize_char(Serializer* s, char n)
{
	serializer_write(s, &n, sizeof(char));
}

inline void serialize_bool(Serializer* s, b8 n)
{
	serializer_write(s, &n, sizeof(b8));
}

inline void serialize_color(Serializer* s, Color n)
{
	serializer_write(s, &n, sizeof(Color));
}

inline void serialize_mat4(Serializer* s, Mat4 matrix)
{
	serializer_write(s, &matrix, sizeof(Mat4));
}

inline void serialize_string(Serializer* s, const char* str)
{
	u32 size = string_size(str);
	serializer_write(s, str, size + 1);
}

inline void serialize_v2(Serializer* s, v2 v)
{
	serialize_f32(s, v.x);
	serialize_f32(s, v.y);
}

inline void serialize_v3(Serializer* s, v3 v)
{
	serialize_f32(s, v.x);
	serialize_f32(s, v.y);
	serialize_f32(s, v.z);
}

inline void serialize_v4(Serializer* s, v4 v)
{
	serialize_f32(s, v.x);
	serialize_f32(s, v.y);
	serialize_f32(s, v.z);
	serialize_f32(s, v.w);
}

inline void serialize_v2_i32(Serializer* s, v2_i32 v)
{
	serialize_i32(s, v.x);
	serialize_i32(s, v.y);
}

inline void serialize_v3_i32(Serializer* s, v3_i32 v)
{
	serialize_i32(s, v.x);
	serialize_i32(s, v.y);
	serialize_i32(s, v.z);
}

inline void serialize_v4_i32(Serializer* s, v4_i32 v)
{
	serialize_i32(s, v.x);
	serialize_i32(s, v.y);
	serialize_i32(s, v.z);
	serialize_i32(s, v.w);
}

// Deserializer

typedef struct {
	u32 version;
	u8* data;
	u32 size;
	u32 cursor;
} Deserializer;

inline void deserializer_read(Deserializer* s, void* data, u32 size)
{
	if (s->cursor + size <= s->size) {
		memory_copy(data, s->data + s->cursor, size);
		s->cursor += size;
	}
	else {
		assert_title(FALSE, "Deserializer overflow");
		memory_zero(data, size);
	}
}

inline b8 deserializer_begin(Deserializer* s, const char* filepath)
{
	s->cursor = 0;
	if (file_read_binary(filepath, &s->data, &s->size)) {

		deserializer_read(s, &s->version, sizeof(u32));
		return TRUE;
	}

	return FALSE;
}

inline void deserializer_end(Deserializer* s)
{
	if (s->data) memory_free(s->data);
}

inline void deserialize_u8(Deserializer* s, u8* n)
{
	deserializer_read(s, n, sizeof(u8));
}

inline void deserialize_u16(Deserializer* s, u16* n)
{
	deserializer_read(s, n, sizeof(u16));
}

inline void deserialize_u32(Deserializer* s, u32* n)
{
	deserializer_read(s, n, sizeof(u32));
}

inline void deserialize_u64(Deserializer* s, u64* n)
{
	deserializer_read(s, n, sizeof(u64));
}

inline void deserialize_size_t(Deserializer* s, size_t* n)
{
	u64 n0;
	deserializer_read(s, &n0, sizeof(u64));
	*n = n0;
}

inline void deserialize_i8(Deserializer* s, i8* n)
{
	deserializer_read(s, n, sizeof(i8));
}

inline void deserialize_i16(Deserializer* s, i16* n)
{
	deserializer_read(s, n, sizeof(i16));
}

inline void deserialize_i32(Deserializer* s, i32* n)
{
	deserializer_read(s, n, sizeof(i32));
}

inline void deserialize_i64(Deserializer* s, i64* n)
{
	deserializer_read(s, n, sizeof(i64));
}

inline void deserialize_f32(Deserializer* s, f32* n)
{
	deserializer_read(s, n, sizeof(f32));
}

inline void deserialize_f64(Deserializer* s, f64* n)
{
	deserializer_read(s, n, sizeof(f64));
}

inline void deserialize_char(Deserializer* s, char* n)
{
	deserializer_read(s, n, sizeof(char));
}

inline void deserialize_bool(Deserializer* s, b8* n)
{
	deserializer_read(s, n, sizeof(b8));
}

inline void deserialize_color(Deserializer* s, Color* n)
{
	deserializer_read(s, n, sizeof(Color));
}

inline void deserialize_mat4(Deserializer* s, Mat4* matrix)
{
	deserializer_read(s, matrix, sizeof(Mat4));
}

inline void deserialize_string(Deserializer* s, char* str, u32 buffer_size)
{
	if (s->cursor < s->size) {
		
		u32 start = s->cursor;
		while (s->data[s->cursor] != '\0' && s->cursor < s->size) {
			++s->cursor;
		}

		if (s->cursor != s->size) {

			memory_copy(str, s->data + start, s->cursor - start + 1);
			return;
		}
	}
	

	assert_title(FALSE, "Deserializer overflow");
	string_copy(str, "", buffer_size);
}

inline void deserialize_v2(Deserializer* s, v2* v)
{
	deserialize_f32(s, &v->x);
	deserialize_f32(s, &v->y);
}

inline void deserialize_v3(Deserializer* s, v3* v)
{
	deserialize_f32(s, &v->x);
	deserialize_f32(s, &v->y);
	deserialize_f32(s, &v->z);
}

inline void deserialize_v4(Deserializer* s, v4* v)
{
	deserialize_f32(s, &v->x);
	deserialize_f32(s, &v->y);
	deserialize_f32(s, &v->z);
	deserialize_f32(s, &v->w);
}

inline void deserialize_v2_i32(Deserializer* s, v2_i32* v)
{
	deserialize_i32(s, &v->x);
	deserialize_i32(s, &v->y);
}

inline void deserialize_v3_i32(Deserializer* s, v3_i32* v)
{
	deserialize_i32(s, &v->x);
	deserialize_i32(s, &v->y);
	deserialize_i32(s, &v->z);
}

inline void deserialize_v4_i32(Deserializer* s, v4_i32* v)
{
	deserialize_i32(s, &v->x);
	deserialize_i32(s, &v->y);
	deserialize_i32(s, &v->z);
	deserialize_i32(s, &v->w);
}

SV_END_C_HEADER
