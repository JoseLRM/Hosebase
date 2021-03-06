#pragma once

#include "Hosebase/graphics.h"
#include "Hosebase/platform.h"

SV_BEGIN_C_HEADER

b8 load_image(FilepathType type, const char* filepath, void** pdata, u32* width, u32* height);

b8 bin_read(u64 hash, u8** data, u32* size);
//b8 bin_read(u64 hash, Deserializer& deserializer, b8 system); // Begins the deserializer

b8 bin_write(u64 hash, const void* data, u32 size);
//b8 bin_write(u64 hash, Serializer& serializer, b8 system); // Ends the serializer

// XML PARSER

typedef struct {

	b8 corrupted;
	const char* data;
	u32 size;

	i32 level;
	const char* begin;
	const char* end;

} XMLElement;

XMLElement xml_begin(const char* data, u32 size);
u32        xml_name(const XMLElement* e, char* buffer, u32 buffer_size);
b8         xml_enter_child(XMLElement* e, const char* name);
b8         xml_next(XMLElement* e);
b8         xml_element_content(XMLElement* e, const char** pbegin, const char** pend);
b8         xml_get_attribute(XMLElement* e, char* buffer, u32 buffer_size, const char* att_name);
b8         xml_get_attribute_u32(XMLElement* e, u32* n, const char* att_name);

// Serializer

#define SERIALIZER_VERSION 0
#define SERIALIZER_ALLOCATE 1000

typedef struct {
	u8* data;
	u32 cursor;
	u32 capacity;
	b8 extern_data;
} Serializer;

SV_INLINE void serializer_prepare(Serializer* s, u32 size)
{
	if (s->cursor + size > s->capacity) {

		u32 new_capacity = SV_MAX(s->cursor + size, s->capacity + SERIALIZER_ALLOCATE);
		u8* new_data = (u8*)memory_allocate(new_capacity);

		if (s->cursor) {
			memory_copy(new_data, s->data, s->cursor);
			if (!s->extern_data)
				memory_free(s->data);
		}

		s->data = new_data;
		s->capacity = new_capacity;
		s->extern_data = FALSE;
	}
}

SV_INLINE void serializer_write(Serializer* s, const void* data, u32 size)
{
	serializer_prepare(s, size);

	memory_copy(s->data + s->cursor, data, size);
	s->cursor += size;
}

SV_INLINE void serializer_begin_file(Serializer* s, u32 initial_capacity)
{
	s->capacity = SV_MAX(initial_capacity, SERIALIZER_ALLOCATE);
	s->data = (u8*)memory_allocate(s->capacity);
	s->cursor = 0u;
	s->extern_data = FALSE;

	u32 version = SERIALIZER_VERSION;
	serializer_write(s, &version, sizeof(u32));
}

SV_INLINE void serializer_begin_buffer(Serializer* s, void* buffer, u32 size)
{
	s->capacity = size;
	s->data = (u8*)buffer;
	s->cursor = 0u;
	s->extern_data = buffer != NULL;
}

SV_INLINE b8 serializer_end_file(Serializer* s, FilepathType type, const char* filepath)
{
	if (s->cursor == 0)
		return FALSE;
	
	b8 res = file_write_binary(type, filepath, s->data, s->cursor, FALSE, TRUE);

	if (s->data && !s->extern_data) memory_free(s->data);

	return res;
}

SV_INLINE void serializer_end_buffer(Serializer* s)
{
	if (s->data && !s->extern_data) memory_free(s->data);
}

SV_INLINE void serialize_u8(Serializer* s, u8 n)
{
	serializer_write(s, &n, sizeof(u8));
}

SV_INLINE void serialize_u16(Serializer* s, u16 n)
{
	serializer_write(s, &n, sizeof(u16));
}

SV_INLINE void serialize_u32(Serializer* s, u32 n)
{
	serializer_write(s, &n, sizeof(u32));
}

SV_INLINE void serialize_u32_array(Serializer* s, const u32* n, u32 count)
{
	serializer_write(s, n, sizeof(u32) * count);
}

SV_INLINE void serialize_u64(Serializer* s, u64 n)
{
	serializer_write(s, &n, sizeof(u64));
}

SV_INLINE void serialize_size_t(Serializer* s, size_t n)
{
	u64 n0 = (u64)n;
	serializer_write(s, &n0, sizeof(u64));
}

SV_INLINE void serialize_i8(Serializer* s, i8 n)
{
	serializer_write(s, &n, sizeof(i8));
}

SV_INLINE void serialize_i16(Serializer* s, i16 n)
{
	serializer_write(s, &n, sizeof(i16));
}

SV_INLINE void serialize_i32(Serializer* s, i32 n)
{
	serializer_write(s, &n, sizeof(i32));
}

SV_INLINE void serialize_i64(Serializer* s, i64 n)
{
	serializer_write(s, &n, sizeof(i64));
}

SV_INLINE void serialize_f32(Serializer* s, f32 n)
{
	serializer_write(s, &n, sizeof(f32));
}

SV_INLINE void serialize_f64(Serializer* s, f64 n)
{
	serializer_write(s, &n, sizeof(f64));
}

SV_INLINE void serialize_char(Serializer* s, char n)
{
	serializer_write(s, &n, sizeof(char));
}

SV_INLINE void serialize_b8(Serializer* s, b8 n)
{
	serializer_write(s, &n, sizeof(b8));
}

SV_INLINE void serialize_color(Serializer* s, Color n)
{
	serializer_write(s, &n, sizeof(Color));
}

SV_INLINE void serialize_m4(Serializer* s, m4 matrix)
{
	serializer_write(s, &matrix, sizeof(m4));
}

SV_INLINE void serialize_string(Serializer* s, const char* str)
{
	u32 size = string_size(str);
	serializer_write(s, str, size + 1);
}

SV_INLINE void serialize_v2(Serializer* s, v2 v)
{
	serialize_f32(s, v.x);
	serialize_f32(s, v.y);
}

SV_INLINE void serialize_v3(Serializer* s, v3 v)
{
	serialize_f32(s, v.x);
	serialize_f32(s, v.y);
	serialize_f32(s, v.z);
}

SV_INLINE void serialize_v4(Serializer* s, v4 v)
{
	serialize_f32(s, v.x);
	serialize_f32(s, v.y);
	serialize_f32(s, v.z);
	serialize_f32(s, v.w);
}

SV_INLINE void serialize_v2_i32(Serializer* s, v2_i32 v)
{
	serialize_i32(s, v.x);
	serialize_i32(s, v.y);
}

SV_INLINE void serialize_v3_i32(Serializer* s, v3_i32 v)
{
	serialize_i32(s, v.x);
	serialize_i32(s, v.y);
	serialize_i32(s, v.z);
}

SV_INLINE void serialize_v4_i32(Serializer* s, v4_i32 v)
{
	serialize_i32(s, v.x);
	serialize_i32(s, v.y);
	serialize_i32(s, v.z);
	serialize_i32(s, v.w);
}

SV_INLINE void serialize_v2_u32(Serializer* s, v2_u32 v)
{
	serialize_u32(s, v.x);
	serialize_u32(s, v.y);
}

SV_INLINE void serialize_v3_u32(Serializer* s, v3_u32 v)
{
	serialize_u32(s, v.x);
	serialize_u32(s, v.y);
	serialize_u32(s, v.z);
}

SV_INLINE void serialize_v4_u32(Serializer* s, v4_u32 v)
{
	serialize_u32(s, v.x);
	serialize_u32(s, v.y);
	serialize_u32(s, v.z);
	serialize_u32(s, v.w);
}

// Deserializer

typedef struct {
	u32 version;
	u8* data;
	u32 size;
	u32 cursor;
	b8 extern_data;
} Deserializer;

SV_INLINE void deserializer_read(Deserializer* s, void* data, u32 size)
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

SV_INLINE void deserializer_ignore(Deserializer* s, u32 size)
{
	s->cursor += size;
	if (s->cursor > s->size) {
		assert_title(FALSE, "Deserializer overflow");
		s->cursor = s->size;
	}
}

SV_INLINE b8 deserializer_begin_file(Deserializer* s, FilepathType type, const char* filepath)
{
	s->cursor = 0;
	s->extern_data = FALSE;
	if (file_read_binary(type, filepath, &s->data, &s->size)) {

		deserializer_read(s, &s->version, sizeof(u32));
		return TRUE;
	}

	return FALSE;
}

SV_INLINE void deserializer_begin_buffer(Deserializer* s, void* buffer, u32 size)
{
	s->cursor = 0;
	s->extern_data = TRUE;
	s->data = (u8*)buffer;
	s->size = size;
	s->version = SERIALIZER_VERSION;
}

SV_INLINE void deserializer_end_file(Deserializer* s)
{
	if (s->data && !s->extern_data) memory_free(s->data);
}

SV_INLINE void deserializer_end_buffer(Deserializer* s)
{
	if (s->data && !s->extern_data) memory_free(s->data);
}

SV_INLINE void deserialize_u8(Deserializer* s, u8* n)
{
	deserializer_read(s, n, sizeof(u8));
}

SV_INLINE void deserialize_u16(Deserializer* s, u16* n)
{
	deserializer_read(s, n, sizeof(u16));
}

SV_INLINE void deserialize_u32(Deserializer* s, u32* n)
{
	deserializer_read(s, n, sizeof(u32));
}

SV_INLINE void deserialize_u32_array(Deserializer* s, u32* n, u32 count)
{
	deserializer_read(s, n, sizeof(u32) * count);
}

SV_INLINE void deserialize_u64(Deserializer* s, u64* n)
{
	deserializer_read(s, n, sizeof(u64));
}

SV_INLINE void deserialize_size_t(Deserializer* s, size_t* n)
{
	u64 n0;
	deserializer_read(s, &n0, sizeof(u64));
	*n = n0;
}

SV_INLINE void deserialize_i8(Deserializer* s, i8* n)
{
	deserializer_read(s, n, sizeof(i8));
}

SV_INLINE void deserialize_i16(Deserializer* s, i16* n)
{
	deserializer_read(s, n, sizeof(i16));
}

SV_INLINE void deserialize_i32(Deserializer* s, i32* n)
{
	deserializer_read(s, n, sizeof(i32));
}

SV_INLINE void deserialize_i64(Deserializer* s, i64* n)
{
	deserializer_read(s, n, sizeof(i64));
}

SV_INLINE void deserialize_f32(Deserializer* s, f32* n)
{
	deserializer_read(s, n, sizeof(f32));
}

SV_INLINE void deserialize_f64(Deserializer* s, f64* n)
{
	deserializer_read(s, n, sizeof(f64));
}

SV_INLINE void deserialize_char(Deserializer* s, char* n)
{
	deserializer_read(s, n, sizeof(char));
}

SV_INLINE void deserialize_b8(Deserializer* s, b8* n)
{
	deserializer_read(s, n, sizeof(b8));
}

SV_INLINE void deserialize_color(Deserializer* s, Color* n)
{
	deserializer_read(s, n, sizeof(Color));
}

SV_INLINE void deserialize_m4(Deserializer* s, m4* matrix)
{
	deserializer_read(s, matrix, sizeof(m4));
}

SV_INLINE void deserialize_string(Deserializer* s, char* str, u32 buffer_size)
{
	if (s->cursor < s->size) {
		
		u32 start = s->cursor;
		while (s->data[s->cursor] != '\0' && s->cursor < s->size) {
			++s->cursor;
		}

		if (s->cursor != s->size) {

			memory_copy(str, s->data + start, s->cursor - start + 1);
			s->cursor++;
			return;
		}
	}
	

	assert_title(FALSE, "Deserializer overflow");
	string_copy(str, "", buffer_size);
}

SV_INLINE void deserialize_v2(Deserializer* s, v2* v)
{
	deserialize_f32(s, &v->x);
	deserialize_f32(s, &v->y);
}

SV_INLINE void deserialize_v3(Deserializer* s, v3* v)
{
	deserialize_f32(s, &v->x);
	deserialize_f32(s, &v->y);
	deserialize_f32(s, &v->z);
}

SV_INLINE void deserialize_v4(Deserializer* s, v4* v)
{
	deserialize_f32(s, &v->x);
	deserialize_f32(s, &v->y);
	deserialize_f32(s, &v->z);
	deserialize_f32(s, &v->w);
}

SV_INLINE void deserialize_v2_i32(Deserializer* s, v2_i32* v)
{
	deserialize_i32(s, &v->x);
	deserialize_i32(s, &v->y);
}

SV_INLINE void deserialize_v3_i32(Deserializer* s, v3_i32* v)
{
	deserialize_i32(s, &v->x);
	deserialize_i32(s, &v->y);
	deserialize_i32(s, &v->z);
}

SV_INLINE void deserialize_v4_i32(Deserializer* s, v4_i32* v)
{
	deserialize_i32(s, &v->x);
	deserialize_i32(s, &v->y);
	deserialize_i32(s, &v->z);
	deserialize_i32(s, &v->w);
}

SV_INLINE void deserialize_v2_u32(Deserializer* s, v2_u32* v)
{
	deserialize_u32(s, &v->x);
	deserialize_u32(s, &v->y);
}

SV_INLINE void deserialize_v3_u32(Deserializer* s, v3_u32* v)
{
	deserialize_u32(s, &v->x);
	deserialize_u32(s, &v->y);
	deserialize_u32(s, &v->z);
}

SV_INLINE void deserialize_v4_u32(Deserializer* s, v4_u32* v)
{
	deserialize_u32(s, &v->x);
	deserialize_u32(s, &v->y);
	deserialize_u32(s, &v->z);
	deserialize_u32(s, &v->w);
}

SV_END_C_HEADER
