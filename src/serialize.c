#include "Hosebase/serialize.h"

#include "Hosebase/memory_manager.h"
#include "Hosebase/math.h"
#include "Hosebase/platform.h"

#define STBI_ASSERT(x) assert(x)
#define STBI_MALLOC(size) memory_allocate(size)
#define STBI_REALLOC(ptr, size) realloc(ptr, size)
#define STBI_FREE(ptr) memory_free(ptr)
#define STB_IMAGE_IMPLEMENTATION

#include "Hosebase/external/stbi_lib.h"

b8 load_image(FilepathType type, const char* filepath, void** pdata, u32* width, u32* height)
{
	u8* bin;
	u32 bin_size;
	if (!file_read_binary(type, filepath, &bin, &bin_size))
		return FALSE;
	
	int w = 0, h = 0, bits = 0;
	void* data = stbi_load_from_memory(bin, bin_size, &w, &h, &bits, 4);
	memory_free(bin);

	* pdata = NULL;
	*width = w;
	*height = h;

	if (data == NULL) return FALSE;
	*pdata = data;
	return TRUE;
}

#define BIN_PATH_SIZE 101
    
inline void bin_filepath(char* buf, u64 hash)
{
	sprintf(buf, "bin/%llu.bin", hash);
}

b8 bin_read(u64 hash, u8** data, u32* size)
{
	char filepath[BIN_PATH_SIZE];
	bin_filepath(filepath, hash);

	return file_read_binary(FilepathType_File, filepath, data, size);
}
    
/*b8 bin_read(u64 hash, Deserializer& deserializer, b8 system)
  {
  char filepath[BIN_PATH_SIZE];
  bin_filepath(filepath, hash, system);
  return deserialize_begin(deserializer, filepath);
  }*/

b8 bin_write(u64 hash, const void* data, u32 size)
{
	char filepath[BIN_PATH_SIZE];
	bin_filepath(filepath, hash);
	return file_write_binary(FilepathType_File, filepath, (u8*)data, size, FALSE, TRUE);
}
    
/*b8 bin_write(u64 hash, Serializer& serializer, b8 system)
  {
  char filepath[BIN_PATH_SIZE];
  bin_filepath(filepath, hash, system);
  return serialize_end(serializer, filepath);
  }*/

///////////////////////////////////////////// XML LOADER ////////////////////////////////////////////

inline b8 xml_string_equals(const char* str0, const char* str1)
{
	while (*str0 != '\0' && *str1 != '\0') {

		if (*str0 == '>' || *str0 == '/' || *str0 == ' ') {
			str0 = " ";
		}
		if (*str1 == '>' || *str1 == '/' || *str1 == ' ') {
			str1 = " ";
		}

		if (*str0 != *str1) {
			return FALSE;
		}

		++str0;
		++str1;
	}

	return *str0 == *str1;
}

inline const char* xml_exit_tag(const char* c)
{
	while (*c != '\0' && *c != '>') {

		if (*c == '/' && *(c + 1) == '>') {
			++c;
			break;
		}

		++c;
	}

	if (*c == '\0') return c;

	return c + 1;
}

inline const char* xml_find_end(const char* begin)
{
	const char* c = begin + 1;
	const char* name_begin = c;
	i32 level = 0;

	// Exit from tag
	{
		while (*c != '\0' && *c != '>') {

			if (*c == '/' && *(c + 1) == '>') {
				break;
			}

			++c;
		}

		if (*c == '\0') return NULL;
		if (*c == '/') {
			return c + 2;
		}
		else {
			++c;
		}
	}

	// Find close tag
	{
		while (1) {
			while (*c != '\0' && *c != '<') {

				if (*c == '/' && *(c + 1) == '>') {
					level--;
					++c;
				}

				++c;
			}

			if (*c == '\0')
				return NULL;

			++c;

			if (*c == '/') {

				++c;

				if (level == 0 && xml_string_equals(name_begin, c)) {

					while (*c != '\0' && *c != '>') {
						++c;
					}

					if (*c == '>') {

						return c + 1;
					}
					else return NULL;
				}
				else {
					level--;
				}
			}
			else level++;
		}
	}

	return begin;
}

inline b8 xml_string_equals_to_normal(const char* xml_str, const char* normal)
{
	while (*xml_str != '\0' && *normal != '\0') {

		if (*xml_str == '/' || *xml_str == '>' || *xml_str == ' ' || *xml_str == '=') {
			xml_str = "";
		}

		if (*xml_str != *normal) {
			return FALSE;
		}

		++xml_str;
		++normal;
	}

	if (*xml_str == '/' || *xml_str == '>' || *xml_str == ' ' || *xml_str == '=') {
		xml_str = "";
	}

	return *xml_str == *normal;
}

XMLElement xml_begin(const char* data, u32 size)
{
	XMLElement e;
	e.data = data;
	e.size = size;
	e.corrupted = FALSE;
	e.level = 0;

	const char* c = line_jump_spaces(e.data);

	while (!e.corrupted) {

		if (*c == '<') {

			e.begin = c;

			++c;

			if (*c == '?') {

				while (*c != '>' && *c != '\0')
					c++;

				if (*c == '>') {
					++c;
				}
				else {
					e.corrupted = TRUE;
				}
			}
			else {

				const char* end = xml_find_end(e.begin);

				if (end) {
					e.end = end;
					break;
				}
				else {
					e.corrupted;
				}
			}
		}
		else if (*c == '\n' || *c == ' ' || *c == '\r' || *c == '\t') {
			++c;
		}
		else {
			e.corrupted = TRUE;
		}
	}

	return e;
}

u32 xml_name(const XMLElement* e, char* buffer, u32 buffer_size)
{
	const char* begin = e->begin + 1;
	const char* end = begin;

	while (*end != '\0' && *end != '/' && *end != ' ' && *end != '>') {
		++end;
	}

	u32 size = end - begin;

	if (buffer_size == 0)
		return size;

	u32 copy = SV_MIN(size, buffer_size - 1);

	memory_copy(buffer, begin, copy);
	buffer[copy] = '\0';

	return size - copy;
}

b8 xml_enter_child(XMLElement* e, const char* name)
{
	const char* c = xml_exit_tag(e->begin);

	while (1) {

		while (*c != '\0' && *c != '<' && *c != '>') {
			++c;
		}

		if (*c == '\0') {
			e->corrupted = TRUE;
			return FALSE;
		}
		else if (*c == '>') {
			return FALSE;
		}
		else if (*(c + 1) == '/') {
			return FALSE;
		}

		++c;

		const char* begin = c - 1;
		const char* end = xml_find_end(begin);

		if (end == NULL) {
			e->corrupted = TRUE;
			return FALSE;
		}

		if (xml_string_equals_to_normal(c, name)) {

			e->begin = begin;
			e->end = end;
			e->level++;
			break;
		}
		else {

			c = end;
		}
	}

	return TRUE;

corrupted:
	e->corrupted = TRUE;
	return FALSE;
}

b8 xml_next(XMLElement* e)
{
	if (e->level == 0)
		return FALSE;

	const char* c = e->end;

	while (1) {

		while (*c != '\0' && *c != '<' && *c != '/') {
			++c;
		}

		if (*c == '\0') {
			e->corrupted = TRUE;
			return FALSE;
		}
		else if (*c == '/') {
			return FALSE;
		}
		else if (*(c + 1) == '/')
			return FALSE;

		++c;

		const char* begin = c - 1;
		const char* end = xml_find_end(begin);

		if (end == NULL) {
			e->corrupted = TRUE;
			return FALSE;
		}

		if (xml_string_equals(c, e->begin + 1)) {

			e->begin = begin;
			e->end = end;
			break;
		}
		else {

			c = end;
		}
	}

	return TRUE;
}

b8 xml_element_content(XMLElement* e, const char** pbegin, const char** pend)
{
	// Begin
	{
		const char* c = e->begin + 1;

		while (*c != '\0' && *c != '>' && *c != '/') {
			++c;
		}

		if (*c == '/')
			return FALSE;
		else if (*c == '\0') {
			e->corrupted = TRUE;
			return FALSE;
		}

		++c;
		*pbegin = c;
	}

	// End
	{
		const char* c = e->end - 1;
		while ((e->data + 4) < c && *c != '/' && *c != '<') {
			--c;
		}

		if (*c == '/' && *(c - 1) == '<') {
			*pend = c - 1;
			return TRUE;
		}
		else {
			e->corrupted = TRUE;
			return FALSE;
		}
	}
}

b8 xml_get_attribute(XMLElement* e, char* buffer, u32 buffer_size, const char* att_name)
{
	if (buffer_size == 0 || e->corrupted)
		return FALSE;

	const char* c = e->begin + 1;
	while (*c != '\0' && *c != '>') {

		if (*c == ' ' || *c == '\n' || *c == '\t') {

			while (*c == ' ' || *c == '\n' || *c == '\t') {
				++c;
			}

			if (*c == '\0' || *c == '>' || *c == '/') {
				return FALSE;
			}

			const char* start_name = c;

			while (*c != '\0' && *c != '>' && *c != '=')
				++c;

			if (*c == '\0' || *c == '>' || *c == '/') {
				return FALSE;
			}

			if (*c == '=' && xml_string_equals_to_normal(start_name, att_name)) {

				++c;
				while (*c == ' ')
					++c;

				const char* begin = c + 1;

				if (*c == '"') {

					++c;

					while (*c != '\0' && *c != '"')
						++c;

					if (*c == '\0') {
						e->corrupted = TRUE;
						return FALSE;
					}

					const char* end = c;

					u32 size = end - begin;
					u32 copy = SV_MIN(size, buffer_size - 1);
					memory_copy(buffer, begin, copy);
					buffer[copy] = '\0';
					return TRUE;
				}
			}
		}

		++c;
	}

	return FALSE;
}

b8 xml_get_attribute_u32(XMLElement* e, u32* n, const char* att_name)
{
	char str[20];
	if (xml_get_attribute(e, str, 20, att_name)) {

		return string_to_u32(n, str);
	}
	return FALSE;
}
