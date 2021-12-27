#pragma once

#include "Hosebase/defines.h"

SV_BEGIN_C_HEADER

#if SV_SLOW

void* __impl__memory_allocate(size_t size, u32 line, const char* file);

#define memory_allocate(size) __impl__memory_allocate(size, __LINE__, __FILE__)
#define memory_allocate_ex(size, line, file) __impl__memory_allocate(size, line, file)

#else

void* memory_allocate(size_t size);

#define memory_allocate_ex(size, line, file) memory_allocate(size)

#endif

void memory_free(void* ptr);
void memory_copy(void* dst, const void* src, size_t size);
void memory_zero(void* dst, size_t size);
void memory_swap(void* p0, void* p1, size_t size);

inline b8 array_prepare(void** data, u32* count, u32* capacity, u32 new_capacity, u32 add, u32 stride)
{
	if (*count + add > * capacity) {

		u8* new_data = (u8*)memory_allocate(new_capacity * stride);

		if (*count) {
			memory_copy(new_data, *data, *count * stride);
			memory_free(*data);
		}

		*data = new_data;
		*capacity = new_capacity;
		return TRUE;
	}
	return FALSE;
}

typedef b8(*LessThanFn)(const void*, const void*);

struct _SortData {
	u8* data;
	u32 stride;
	LessThanFn fn;
};

static void _insertion_sort(struct _SortData* data, u32 begin, u32 end)
{
	for (u32 i = begin + 1; i < end; ++i) {

		i32 j = i - 1;

		while (1) {

			if (!data->fn(data->data + (i * data->stride), data->data + (j * data->stride))) {
				++j;
				break;
			}

			--j;

			if (j == begin - 1) {
				j = begin;
				break;
			}
		}

		if (j != i) {

			for (u32 w = j; w < i; ++w) {

				memory_swap(data->data + (i * data->stride), data->data + (w * data->stride), data->stride);
			}
		}
	}
}

static void _quick_sort(struct _SortData* data, u32 left_limit, u32 right_limit)
{
	i32 left = left_limit;

	i32 right = right_limit;
	u32 pivote = right;
	--right;

	while (1) {

		while (data->fn(data->data + (left * data->stride), data->data + (pivote * data->stride)) && left < right) ++left;
		while (data->fn(data->data + (pivote * data->stride), data->data + (right * data->stride)) && right > left) --right;

		if (left < right) {

			memory_swap(data->data + (left * data->stride), data->data + (right * data->stride), data->stride);
			++left;
			--right;
		}
		else break;
	}

	if (left < right)
		++left;

	if (!data->fn(data->data + (left * data->stride), data->data + (pivote * data->stride))) {

		memory_swap(data->data + (left * data->stride), data->data + (right_limit * data->stride), data->stride);
	}

	if (left_limit != left) {

		if (left - left_limit <= 100) {
			_insertion_sort(data, left_limit, left + 1);
		}
		else _quick_sort(data, left_limit, left);
	}
	if (left + 1 != right_limit) {

		if (right_limit - (left + 1) <= 100) {
			_insertion_sort(data, left + 1, right_limit + 1);
		}
		else _quick_sort(data, left + 1, right_limit);
	}
}

inline void array_sort(void* data, u32 count, u32 stride, void* fn)
{
	if (data == NULL || count == 0)
		return;

	struct _SortData d;
	d.data = (u8*)data;
	d.stride = stride;
	d.fn = (LessThanFn)fn;

	if (count > 100 && count < 15000) {

		_quick_sort(&d, 0, count - 1);
	}
	else {
		// TODO: Fastest algorithm for biggest array lengths
		_insertion_sort(&d, 0, count);
	}
}

inline const char* string_validate(const char* str)
{
	return str ? str : "";
}

inline u32 string_split(const char* line, char* delimiters, u32 count)
{
	const char* it = line;
	
	while (*it != '\0') {
		
		b8 end = FALSE;
		
		foreach(i, count)
			if (delimiters[i] == *it) {
				end = TRUE;
				break;
			}
		
		if (end) break;
	    
		++it;
	}
	
	u32 size = it - line;
	
	return size;
}

inline u32 string_size(const char* str)
{
	u32 size = 0u;
	while (*str++) ++size;
	return size;
}

inline b8 string_begins(const char* s0, const char* s1)
{
	while (*s0 && *s1) {
		if (*s0 != *s1)
			return FALSE;
		
		++s0;
		++s1;
	}
	
	return *s0 == *s1 || *s1 == '\0';
}

inline b8 string_equals(const char* s0, const char* s1)
{
	while (*s0 && *s1) {
		if (*s0 != *s1)
			return FALSE;
		
		++s0;
		++s1;
	}
	
	return *s0 == *s1;
}

inline u32 string_append(char* dst, const char* src, u32 buff_size)
{
	u32 src_size = string_size(src);
	u32 dst_size = string_size(dst);

	u32 new_size = src_size + dst_size;

	u32 overflows = (buff_size < (new_size + 1u)) ? (new_size + 1u) - buff_size : 0u;

	u32 append_size = (overflows > src_size) ? 0u : (src_size - overflows);

	memory_copy(dst + dst_size, src, append_size);
	new_size = dst_size + append_size;
	dst[new_size] = '\0';
	
	return overflows;
}

inline u32 string_append_char(char* dst, char c, u32 buff_size)
{
	// TODO: Optimize
	char src[2];
	src[0] = c;
	src[1] = '\0';
	return string_append(dst, src, buff_size);
}

inline void string_erase(char* str, u32 index)
{
	u32 size = string_size(str);
	
	char* it = str + index + 1u;
	char* end = str + size;
	
	while (it < end) {
		
		*(it - 1u) = *it;
		++it;
	}
	*(end - 1u) = '\0';
}

inline u32 string_set(char* dst, const char* src, u32 src_size, u32 buff_size)
{
	u32 size = SV_MIN(buff_size - 1u, src_size);
	memory_copy(dst, src, size);
	dst[size] = '\0';
	return (src_size > buff_size - 1u) ? (src_size - buff_size - 1u) : 0u;
}

inline u32 string_copy(char* dst, const char* src, u32 buff_size)
{
	u32 src_size = string_size(src);
	return string_set(dst, src, src_size, buff_size);
}

inline u32 string_insert(char* dst, const char* src, u32 index, u32 buff_size)
{
	if (buff_size <= index)
		return 0u;
	
	u32 src_size = string_size(src);
	u32 dst_size = string_size(dst);
	
	index = SV_MIN(dst_size, index);

	u32 moved_index = index + src_size;
	if (moved_index < buff_size - 1u) {

		u32 move_size = SV_MIN(dst_size - index, buff_size - moved_index - 1u);

		char* end = dst + moved_index - 1u;
		char* it0 = dst + moved_index + move_size;
		char* it1 = dst + index + move_size;

		while (it0 != end) {

			*it0 = *it1;
			--it1;
			--it0;
		}
	}

	u32 src_cpy = SV_MIN(src_size, buff_size - 1u - index);
	memory_copy(dst + index, src, src_cpy);

	u32 final_size = SV_MIN(buff_size - 1u, src_size + dst_size);
	dst[final_size] = '\0';

	if (src_size + dst_size > buff_size - 1u) {
		return (src_size - dst_size) - (buff_size - 1u);
	}
	return 0u;
}

inline void string_from_u32(char* dst, u32 value)
{
	u32 digits = 0u;

	u32 aux = value;
	while (aux != 0) {
		aux /= 10;
		++digits;
	}

	if (digits == 0u) {
		string_copy(dst, "0", 11);
		return;
	}

	i32 end = (i32)digits - 1;
	
	for (i32 i = end; i >= 0; --i) {

		u32 v = value % 10;

		switch (v) {

		case 0:
			dst[i] = '0';
			break;

		case 1:
			dst[i] = '1';
			break;

		case 2:
			dst[i] = '2';
			break;

		case 3:
			dst[i] = '3';
			break;

		case 4:
			dst[i] = '4';
			break;

		case 5:
			dst[i] = '5';
			break;
			
		case 6:
			dst[i] = '6';
			break;

		case 7:
			dst[i] = '7';
			break;

		case 8:
			dst[i] = '8';
			break;

		case 9:
			dst[i] = '9';
			break;
			
		}
		
		value /= 10;
	}

	dst[end + 1] = '\0';
}

inline void string_from_u64(char* dst, u64 value)
{
	u32 digits = 0u;

	u64 aux = value;
	while (aux != 0) {
		aux /= 10;
		++digits;
	}

	if (digits == 0u) {
		string_copy(dst, "0", 20);
		return;
	}

	i32 end = (i32)digits - 1;

	for (i32 i = end; i >= 0; --i) {

		u64 v = value % 10;

		switch (v) {

		case 0:
			dst[i] = '0';
			break;

		case 1:
			dst[i] = '1';
			break;

		case 2:
			dst[i] = '2';
			break;

		case 3:
			dst[i] = '3';
			break;

		case 4:
			dst[i] = '4';
			break;

		case 5:
			dst[i] = '5';
			break;

		case 6:
			dst[i] = '6';
			break;

		case 7:
			dst[i] = '7';
			break;

		case 8:
			dst[i] = '8';
			break;

		case 9:
			dst[i] = '9';
			break;

		}

		value /= 10;
	}

	dst[end + 1] = '\0';
}

inline b8 string_to_u32(u32* dst, char* str)
{
	u32 digits = string_size(str);
	*dst = 0u;

	if (digits == 0) return FALSE;

	u32 mul = 10;
	foreach(i, digits - 1)
			mul *= 10;

	foreach(i, digits) {

		mul /= 10;
		
		char c = str[i];
		u32 v;

		switch (c) {
			
		case '0':
			v = 0;
			break;
		case '1':
			v = 1;
			break;
		case '2':
			v = 2;
			break;
		case '3':
			v = 3;
			break;
		case '4':
			v = 4;
			break;
		case '5':
			v = 5;
			break;
		case '6':
			v = 6;
			break;
		case '7':
			v = 7;
			break;
		case '8':
			v = 8;
			break;
		case '9':
			v = 9;
			break;

		default:
			return FALSE;
			
		}

		v *= mul;
		*dst += v;
	}

	return TRUE;
}

inline b8 string_to_u32_hexadecimal(u32* dst, char* str)
{
	u32 digits = string_size(str);
	*dst = 0u;

	if (digits == 0) return FALSE;

	foreach(i, digits) {
		
		char c = str[i];
		u32 v;

		switch (c) {
			
		case '0':
			v = 0;
			break;
		case '1':
			v = 1;
			break;
		case '2':
			v = 2;
			break;
		case '3':
			v = 3;
			break;
		case '4':
			v = 4;
			break;
		case '5':
			v = 5;
			break;
		case '6':
			v = 6;
			break;
		case '7':
			v = 7;
			break;
		case '8':
			v = 8;
			break;
		case '9':
			v = 9;
			break;
		case 'A':
		case 'a':
			v = 10;
			break;
		case 'B':
		case 'b':
			v = 11;
			break;
		case 'C':
		case 'c':
			v = 12;
			break;
		case 'D':
		case 'd':
			v = 13;
			break;
		case 'E':
		case 'e':
			v = 14;
			break;
		case 'F':
		case 'f':
			v = 15;
			break;

		default:
			return FALSE;
			
		}

		v *= digits - i;
		*dst += v;
	}

	return TRUE;
}

inline const char* filepath_extension(const char* filepath)
{
	const char* last_dot = NULL;
	
	const char* it = filepath;
	
	while (*it) {
		
		switch (*it) {
			
		case '/':
			last_dot = NULL;
			break;
			
		case '.':
			last_dot = it;
			break;
			
		}
	    
		++it;
	}
	
	if (last_dot) {
		
		if (last_dot == filepath)
			last_dot = NULL;
	    
		else if (*(last_dot - 1u) == '/')
			last_dot = NULL;
	}
	
	return last_dot;
}

inline const char* filepath_name(const char* filepath)
{
	u32 s = string_size(filepath);
	
	if (s) {
		
		--s;
		while (s && filepath[s] != '/') --s;
		
		if (s) {
			return filepath + s + 1u;
		}
	}
	
	return filepath;
}

inline u32 filepath_folder(const char* filepath)
{
	u32 size = string_size(filepath);
	while (size != 0 && filepath[size] != '/') --size;
	return size;
}

inline b8 char_is_letter(u32 c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

inline b8 char_is_number(u32 c)
{
	return c >= '0' && c <= '9';
}

inline b8 char_is_lower_case(u32 c)
{
	return c >= 'a' && c <= 'z';
}

inline b8 char_is_capital(u32 c)
{
	return c >= 'A' && c <= 'Z';
}

// Line processing

inline const char* line_next(const char* it)
{
	while (*it != '\n' && *it != '\0')
		++it;

	if (*it == '\n')
		++it;

	return it;
}

inline const char* line_jump_spaces(const char* it)
{
	while (*it == ' ')
		++it;
	return it;
}

inline const char* line_read_f32(const char* it, f32* value, b8* res)
{
	*value = 0.f;
	it = line_jump_spaces(it);
	
	if (res) *res = FALSE;
	const char* start = it;

	if (*it == '-') ++it;
	
	u32 dots = 0u;
	
	while (*it != '\0' && *it != ' ' && *it != '\n' && *it != '\r') {
		if (!char_is_number(*it) && *it != '.')
			return start;
		
		if (*it == '.')
			++dots;
		
		++it;
	}
	
	if (it == start || dots > 1) return start;
	
	u32 size = it - start;	
	
	char value_str[20u];
	memory_copy(value_str, start, size);
	value_str[size] = '\0';
	// TODO: Use my own function
	*value = (f32)atof(value_str);
	
	if (res) *res = TRUE;
	
	return it;
}

inline const char* line_read_i32(const char* it, i32* value, const char* delimiters, u32 delimiter_count, b8* pres)
{
	*value = 0;
	it = line_jump_spaces(it);

	if (pres) *pres = FALSE;
	const char* start = it;
	
	if (*it == '-') ++it;
	
	while (*it != '\0' && *it != '\n' && *it != '\r') {
		
		b8 end = FALSE;
		
		foreach(i, delimiter_count)
			if (delimiters[i] == *it) end = TRUE;
		
		if (end)
			break;
	    
		if (!char_is_number(*it))
			return start;
		++it;
	}
	
	if (it == start) return start;
	
	u32 size = it - start;
	
	char value_str[20u];
	memory_copy(value_str, start, size);
	value_str[size] = '\0';
	// TODO: Use my own function
	*value = atoi(value_str);
	
	if (pres) *pres = TRUE;
	
	return it;
}

inline const char* line_read_u32(const char* it, u32* value, const char* delimiters, u32 delimiter_count, b8* pres)
{
	*value = 0;
	it = line_jump_spaces(it);

	if (pres)* pres = FALSE;
	const char* start = it;

	while (*it != '\0' && *it != '\n' && *it != '\r') {

		b8 end = FALSE;

		foreach(i, delimiter_count)
			if (delimiters[i] == *it) end = TRUE;

		if (end)
			break;

		if (!char_is_number(*it))
			return start;
		++it;
	}

	if (it == start) return start;

	u32 size = it - start;

	char value_str[20u];
	memory_copy(value_str, start, size);
	value_str[size] = '\0';

	string_to_u32(value, value_str);
	
	if (pres) *pres = TRUE;

	return it;
}

inline const char* line_read_v3(const char* it, v3* value, b8* res)
{
	it = line_read_f32(it, &value->x, res);
	if (*res) it = line_read_f32(it, &value->y, res);
	if (*res) it = line_read_f32(it, &value->z, res);
	return it;
}


/*

inline b8 string_modify(char* dst, size_t buff_size, u32& cursor, bool* _enter)
{
	b8 modified = FALSE;
	b8 enter = FALSE;
		
	if (buff_size >= 2u) {
		
		size_t max_line_size = buff_size - 1u;
		
		if (input.text[0] && cursor < max_line_size) {
			
			size_t current_size = string_size(dst);
			size_t append_size = string_size(input.text);
			
			if (current_size + append_size > max_line_size)
				append_size = max_line_size - current_size;

			if (append_size) {

				size_t overflow = string_insert(dst, input.text, cursor, buff_size);
	    
				cursor += u32(string_size(input.text) - overflow);
				modified = TRUE;
			}
		}
		
		foreach(i, input.text_commands.size()) {
			
			TextCommand txtcmd = input.text_commands[i];

			switch (txtcmd)
			{
			case TextCommand_DeleteLeft:
				if (cursor) {

					--cursor;
					string_erase(dst, cursor);
				}
				break;
			case TextCommand_DeleteRight:
			{
				size_t size = strlen(dst);
				if (cursor < size) {
		    
					string_erase(dst, cursor);
				}
			}
			break;
		
			case TextCommand_Enter:
			{
				enter = true;
			}
			break;

			case TextCommand_Escape:
				break;
			}
		}

		if (input.keys[Key_Left] == InputState_Pressed) cursor = (cursor == 0u) ? cursor : (cursor - 1u);
		if (input.keys[Key_Right] == InputState_Pressed) cursor = (cursor == strlen(dst)) ? cursor : (cursor + 1u);
	}

	if (_enter) *_enter = enter;

	return modified;
}

inline const char* filepath_name(const char* filepath)
{
	size_t s = strlen(filepath);
	
	if (s) {

		--s;
		while (s && filepath[s] != '/') --s;

		if (s) {
			return filepath + s + 1u;
		}
	}
	
	return filepath;
}

inline char* filepath_name(char* filepath)
{
	size_t s = strlen(filepath);

	if (s) {

		--s;
		while (s && filepath[s] != '/') --s;

		if (s) {
			return filepath + s + 1u;
		}
	}
	
	return filepath;
}

inline char* filepath_extension(char* filepath)
{
	char* last_dot = nullptr;
	
	char* it = filepath;
	
	while (*it) {
		
		switch (*it) {

			case '/':
				last_dot = nullptr;
				break;

			case '.':
				last_dot = it;
				break;
		
			}
	    
			++it;
		}

		if (last_dot) {

			if (last_dot == filepath)
				last_dot = nullptr;
	    
			else if (*(last_dot - 1u) == '/')
				last_dot = nullptr;
		}

		return last_dot;
    }

    constexpr const char* filepath_extension(const char* filepath)
    {
		const char* last_dot = nullptr;

		const char* it = filepath;

		while (*it) {

			switch (*it) {

			case '/':
				last_dot = nullptr;
				break;

			case '.':
				last_dot = it;
				break;
		
			}
	    
			++it;
		}

		if (last_dot) {

			if (last_dot == filepath)
				last_dot = nullptr;
	    
			else if (*(last_dot - 1u) == '/')
				last_dot = nullptr;
		}

		return last_dot;
    }
    
    // Character utils
    
    constexpr bool char_is_letter(char c) 
    {
		return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
    }
    
    constexpr bool char_is_number(char c)
    {
		return c >= '0' && c <= '9';
    }

    // Line processing

    struct LineProcessor {
		String line_buff;
		const char* str;
		size_t offset;
		u32 line_count;
		const char* line;
    };
    
    SV_INLINE void line_begin(LineProcessor& processor, const char* str)
    {
		processor.str = str;
		processor.offset = 0u;
		processor.line_count = 0u;
    }

    SV_INLINE bool line_next(LineProcessor& processor)
    {
		processor.offset += processor.line_buff.size();

		char del = '\n';
		size_t size = string_split(processor.str + processor.offset, &del, 1u);
		if (*(processor.str + processor.offset + size) != '\0')
			++size;

		if (size) {

			processor.line_buff.reset();
			processor.line_buff.append(processor.str, processor.offset, size);
			processor.line = processor.line_buff.c_str();
			++processor.line_count;
			return true;
		}
		else {
			processor.line_buff.clear();
			processor.line = nullptr;
			return false;
		}
    }

    SV_INLINE void line_jump_spaces(const char*& line)
    {
		while (*line == ' ') ++line;
    }

    SV_INLINE bool line_read_f32(const char*& line, f32& value)
    {
		value = 0.f;
		line_jump_spaces(line);

		const char* start = line;

		if (*line == '-') ++line;

		u32 dots = 0u;
	
		while (*line != '\0' && *line != ' ' && *line != '\n' && *line != '\r') {
			if (!char_is_number(*line) && *line != '.')
				return false;

			if (*line == '.')
				++dots;
	    
			++line;
		}

		if (line == start || dots > 1) return false;
	
		size_t size = line - start;	

		char value_str[20u];
		memory_copy(value_str, start, size);
		value_str[size] = '\0';
		value = (f32)atof(value_str);

		return true;
    }

	SV_INLINE bool line_read_f64(const char*& line, f64& value)
    {
		value = 0.f;
		line_jump_spaces(line);

		const char* start = line;

		if (*line == '-') ++line;

		u32 dots = 0u;
	
		while (*line != '\0' && *line != ' ' && *line != '\n' && *line != '\r') {
			if (!char_is_number(*line) && *line != '.')
				return false;

			if (*line == '.')
				++dots;
	    
			++line;
		}

		if (line == start || dots > 1) return false;
	
		size_t size = line - start;	

		char value_str[50u];
		memory_copy(value_str, start, size);
		value_str[size] = '\0';
		char* si;
		value = (f64)strtod(value_str, &si);

		return true;
    }

    SV_INLINE bool line_read_i32(const char*& line, i32& value, const char* delimiters, u32 delimiter_count)
    {
		value = 0;
		line_jump_spaces(line);

		const char* start = line;
	
		if (*line == '-') ++line;
	
		while (*line != '\0' && *line != '\n' && *line != '\r') {

			bool end = false;

			foreach(i, delimiter_count)
				if (delimiters[i] == *line) end = true;

			if (end)
				break;
	    
			if (!char_is_number(*line))
				return false;
			++line;
		}

		if (line == start) return false;
	
		size_t size = line - start;	

		char value_str[20u];
		memory_copy(value_str, start, size);
		value_str[size] = '\0';
		value = atoi(value_str);

		return true;
    }

	SV_INLINE bool line_read_i64(const char*& line, i64& value, const char* delimiters, u32 delimiter_count)
    {
		value = 0;
		line_jump_spaces(line);

		const char* start = line;
	
		if (*line == '-') ++line;
	
		while (*line != '\0' && *line != '\n' && *line != '\r') {

			bool end = false;

			foreach(i, delimiter_count)
				if (delimiters[i] == *line) end = true;

			if (end)
				break;
	    
			if (!char_is_number(*line))
				return false;
			++line;
		}

		if (line == start) return false;
	
		size_t size = line - start;	

		char value_str[50u];
		memory_copy(value_str, start, size);
		value_str[size] = '\0';
		value = atol(value_str);

		return true;
    }

    SV_INLINE bool line_read_v3_f32(const char*& line, v3_f32& value)
    {
		bool res = line_read_f32(line, value.x);
		if (res) res = line_read_f32(line, value.y);
		if (res) res = line_read_f32(line, value.z);
		return res;
    }
    
*/

SV_END_C_HEADER
