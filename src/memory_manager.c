#include "Hosebase/memory_manager.h"

#if SV_SLOW

// TODO: Move on
#include "Hosebase/os.h"
void throw_assertion(const char* title, u32 line, const char* file)
{
	char line_str[20];
	string_from_u32(line_str, line);
	
	char content[1000];
	string_copy(content, title, 1000);
	string_append(content, "\nLine: ", 1000);
	string_append(content, line_str, 1000);
	string_append(content, "\nFile: ", 1000);
	string_append(content, file, 1000);
	
	show_message("Assertion Failed!", content, TRUE);
}

void* __impl__memory_allocate(size_t size, u32 line, const char* file)
{
	void* ptr = NULL;
	while (ptr == NULL) ptr = calloc(1, size);
	return ptr;
}

#else

void* memory_allocate(size_t size)
{
	void* ptr = NULL;
	while (ptr == NULL) ptr = calloc(1, size);
	return ptr;
}

#endif

void memory_free(void* ptr)
{
	if (ptr)
		free(ptr);
}

void memory_copy(void* dst, const void* src, size_t size)
{
	// TODO: Optimize?
	
	u8* dst_it = (u8*)dst;
	const u8* src_it = (const u8*)src;

	foreach(i, size) {
		dst_it[i] = src_it[i];
	}
}

void memory_zero(void* dst, size_t size)
{
	// TODO: Optimize?
	
	u8* it = (u8*)dst;
	u8* end = it + size;
	while (it != end) {
		*it = 0;
		++it;
	}
}

void memory_swap(void* p0, void* p1, size_t size)
{
	// TODO: Optimize?
	
	u8* it0 = (u8*)p0;
	u8* it1 = (u8*)p1;
	u8* end = it0 + size;

	while (it0 != end) {

		u8 aux = *it0;
		*it0 = *it1;
		*it1 = aux;
		
		++it0;
		++it1;
	}
}
