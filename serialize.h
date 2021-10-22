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
		memory_copy(s->data + s->cursor, data, size);
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

/*
constexpr u32 VARNAME_SIZE = 30u;
constexpr u32 VARVALUE_SIZE = 30u;

    enum VarType : u32 {
		VarType_Null,
	    VarType_String,
	    VarType_Integer,
	    VarType_Real,
		VarType_Boolean,
	};
    
    struct Var {
		char name[VARNAME_SIZE + 1u];
		VarType type;
		union {
			char string[VARVALUE_SIZE + 1u];
			i64 integer;
			f64 real;
			bool boolean;
		} value;
    };

    bool read_var_file(const char* filepath, List<Var>& vars);

    /////////////////////////// SERIALIZER //////////////////////////////////

    struct Serializer {
		static constexpr u32 VERSION = 0u;
		RawList buff;
    };

    SV_API void serialize_begin(Serializer& s);
    SV_API bool serialize_end(Serializer& s, const char* filepath);

   

	SV_INLINE void serialize_f32_array(Serializer& s, const f32* v, u32 count)
    {
		s.buff.reserve(sizeof(f32) * count + sizeof(u32));

		serialize_u32(s, count);

		const f32* end = v + size_t(count);

		while (v != end)  {
	
			serialize_f32(s, *v);
			++v;
		}
    }
    SV_INLINE void serialize_v2_f32_array(Serializer& s, const v2_f32* v, u32 count)
    {
		s.buff.reserve(sizeof(f32) * 2u * count + sizeof(u32));

		serialize_u32(s, count);

		const v2_f32* end = v + size_t(count);

		while (v != end)  {
	
			serialize_f32(s, v->x);
			serialize_f32(s, v->y);
			++v;
		}
    }
    SV_INLINE void serialize_v3_f32_array(Serializer& s, const v3_f32* v, u32 count)
    {
		s.buff.reserve(sizeof(f32) * 3u * count + sizeof(u32));

		serialize_u32(s, count);

		const v3_f32* end = v + size_t(count);

		while (v != end)  {
	
			serialize_f32(s, v->x);
			serialize_f32(s, v->y);
			serialize_f32(s, v->z);
			++v;
		}
    }
    SV_INLINE void serialize_v4_f32_array(Serializer& s, const v4_f32* v, u32 count)
    {
		s.buff.reserve(sizeof(f32) * 4u * count + sizeof(u32));

		serialize_u32(s, count);

		const v4_f32* end = v + size_t(count);

		while (v != end)  {
	
			serialize_f32(s, v->x);
			serialize_f32(s, v->y);
			serialize_f32(s, v->z);
			serialize_f32(s, v->w);
			++v;
		}
    }

    SV_INLINE void serialize_u32_array(Serializer& s, const u32* n, u32 count)
    {
		s.buff.reserve(sizeof(u32) * (count + 1u));
		serialize_u32(s, count);

		foreach(i, count)
			serialize_u32(s, n[i]);
    }

	SV_INLINE void serialize_f32_array(Serializer& s, const List<f32>& list)
    {
		serialize_f32_array(s, list.data(), (u32)list.size());
    }
    SV_INLINE void serialize_v2_f32_array(Serializer& s, const List<v2_f32>& list)
    {
		serialize_v2_f32_array(s, list.data(), (u32)list.size());
    }
    SV_INLINE void serialize_v3_f32_array(Serializer& s, const List<v3_f32>& list)
    {
		serialize_v3_f32_array(s, list.data(), (u32)list.size());
    }
    SV_INLINE void serialize_v4_f32_array(Serializer& s, const List<v4_f32>& list)
    {
		serialize_v4_f32_array(s, list.data(), (u32)list.size());
    }

    SV_INLINE void serialize_u32_array(Serializer& s, const List<u32>& list)
    {
		serialize_u32_array(s, list.data(), (u32)list.size());
    }

    SV_INLINE void serialize_version(Serializer& s, Version n)
    {
		s.buff.write_back(&n, sizeof(Version));
    }

    /////////////////////////// DESERIALIZER //////////////////////////////////

    struct Deserializer {
		static constexpr u32 LAST_VERSION_SUPPORTED = 0u;
		u32 serializer_version;
		Version engine_version;
		RawList buff;
		size_t pos;
    };

    SV_API bool deserialize_begin(Deserializer& d, const char* filepath);
    SV_API void deserialize_end(Deserializer& d);

    SV_INLINE bool deserialize_assert(Deserializer& d, size_t size)
    {
		return (d.pos + size) <= d.buff.size();
    }

    SV_INLINE void deserialize_u8(Deserializer& d, u8& n)
    {
		d.buff.read_safe(&n, sizeof(u8), d.pos);
		d.pos += sizeof(u8);
    }
    SV_INLINE void deserialize_u16(Deserializer& d, u16& n)
    {
		d.buff.read_safe(&n, sizeof(u16), d.pos);
		d.pos += sizeof(u16);
    }
    SV_INLINE void deserialize_u32(Deserializer& d, u32& n)
    {
		d.buff.read_safe(&n, sizeof(u32), d.pos);
		d.pos += sizeof(u32);
    }
    SV_INLINE void deserialize_u64(Deserializer& d, u64& n)
    {
		d.buff.read_safe(&n, sizeof(u64), d.pos);
		d.pos += sizeof(u64);
    }
    SV_INLINE void deserialize_size_t(Deserializer& d, size_t& n)
    {
		u64 n0;
		d.buff.read_safe(&n0, sizeof(u64), d.pos);
		d.pos += sizeof(u64);
		n = size_t(n0);
    }

    SV_INLINE void deserialize_i8(Deserializer& d, i8& n)
    {
		d.buff.read_safe(&n, sizeof(i8), d.pos);
		d.pos += sizeof(i8);
    }
    SV_INLINE void deserialize_i16(Deserializer& d, i16& n)
    {
		d.buff.read_safe(&n, sizeof(i16), d.pos);
		d.pos += sizeof(i16);
    }
    SV_INLINE void deserialize_i32(Deserializer& d, i32& n)
    {
		d.buff.read_safe(&n, sizeof(i32), d.pos);
		d.pos += sizeof(i32);
    }
    SV_INLINE void deserialize_i64(Deserializer& d, i64& n)
    {
		d.buff.read_safe(&n, sizeof(u64), d.pos);
		d.pos += sizeof(u64);
    }

    SV_INLINE void deserialize_f32(Deserializer& d, f32& n)
    {
		d.buff.read_safe(&n, sizeof(f32), d.pos);
		d.pos += sizeof(f32);
    }
    SV_INLINE void deserialize_f64(Deserializer& d, f64& n)
    {
		d.buff.read_safe(&n, sizeof(f64), d.pos);
		d.pos += sizeof(f64);
    }

    SV_INLINE void deserialize_char(Deserializer& d, char& n)
    {
		d.buff.read_safe(&n, sizeof(char), d.pos);
		d.pos += sizeof(char);
    }
    SV_INLINE void deserialize_bool(Deserializer& d, bool& n)
    {
		d.buff.read_safe(&n, sizeof(bool), d.pos);
		d.pos += sizeof(bool);
    }

    SV_INLINE void deserialize_color(Deserializer& d, Color& n)
    {
		d.buff.read_safe(&n, sizeof(Color), d.pos);
		d.pos += sizeof(Color);
    }

    SV_INLINE void deserialize_xmmatrix(Deserializer& d, XMMATRIX& n)
    {
		d.buff.read_safe(&n, sizeof(XMMATRIX), d.pos);
		d.pos += sizeof(XMMATRIX);
    }

    SV_INLINE size_t deserialize_string_size(Deserializer& d)
    {
		size_t size;
		const char* str = (const char*)(d.buff.data() + d.pos);
		size = strlen(str);
		return size;
    }
    SV_INLINE void deserialize_string(Deserializer& d, char* str, size_t buff_size)
    {
		size_t size = deserialize_string_size(d);

		size_t read = SV_MIN(size, buff_size - 1u);
		memcpy(str, d.buff.data() + d.pos, read);
		str[read] = '\0';
		d.pos += size + 1u;
    }

    SV_INLINE void deserialize_v2_f32(Deserializer& d, v2_f32& v)
    {
		deserialize_f32(d, v.x);
		deserialize_f32(d, v.y);
    }
    SV_INLINE void deserialize_v3_f32(Deserializer& d, v3_f32& v)
    {
		deserialize_f32(d, v.x);
		deserialize_f32(d, v.y);
		deserialize_f32(d, v.z);
    }
    SV_INLINE void deserialize_v4_f32(Deserializer& d, v4_f32& v)
    {
		deserialize_f32(d, v.x);
		deserialize_f32(d, v.y);
		deserialize_f32(d, v.z);
		deserialize_f32(d, v.w);
    }
	SV_INLINE void deserialize_v2_u32(Deserializer& d, v2_u32& v)
    {
		deserialize_u32(d, v.x);
		deserialize_u32(d, v.y);
    }

	SV_INLINE void deserialize_f32_array(Deserializer& d, List<f32>& list)
    {
		u32 count;
		deserialize_u32(d, count);

		list.resize(count);

		foreach(i, count)  {

			deserialize_f32(d, list[i]);
		}
    }
    SV_INLINE void deserialize_v2_f32_array(Deserializer& d, List<v2_f32>& list)
    {
		u32 count;
		deserialize_u32(d, count);

		list.resize(count);

		foreach(i, count)  {

			v2_f32& v = list[i];
			deserialize_f32(d, v.x);
			deserialize_f32(d, v.y);
		}
    }
    SV_INLINE void deserialize_v3_f32_array(Deserializer& d, List<v3_f32>& list)
    {
		u32 count;
		deserialize_u32(d, count);

		list.resize(count);

		foreach(i, count)  {

			v3_f32& v = list[i];
			deserialize_f32(d, v.x);
			deserialize_f32(d, v.y);
			deserialize_f32(d, v.z);
		}
    }
    SV_INLINE void deserialize_v4_f32_array(Deserializer& d, List<v4_f32>& list)
    {
		u32 count;
		deserialize_u32(d, count);

		list.resize(count);

		foreach(i, count)  {

			v4_f32& v = list[i];
			deserialize_f32(d, v.x);
			deserialize_f32(d, v.y);
			deserialize_f32(d, v.z);
			deserialize_f32(d, v.w);
		}
    }

    SV_INLINE void deserialize_u32_array(Deserializer& d, List<u32>& list)
    {
		u32 count;
		deserialize_u32(d, count);

		list.resize(count);

		foreach(i, count)  {
	    
			deserialize_u32(d, list[i]);
		}
    }

    SV_INLINE void deserialize_version(Deserializer& d, Version& n)
    {
		d.buff.read_safe(&n, sizeof(Version), d.pos);
		d.pos += sizeof(Version);
    }    
    
*/

SV_END_C_HEADER
