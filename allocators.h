#pragma once

#include "Hosebase/memory_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

// DYNAMIC ARRAY

typedef struct {

	u32 size;
	u32 capacity;
	f32 scale_factor;
	
} DynamicArrayHeader;

inline void* __impl__array_init(u32 stride, u32 initial_capacity, f32 scale_factor, u32 line, const char* file)
{
	DynamicArrayHeader* array = (DynamicArrayHeader*)__impl__memory_allocate(sizeof(DynamicArrayHeader) + (stride * initial_capacity), line, file);
	array->size = 0u;
	array->capacity = initial_capacity;
	array->scale_factor = scale_factor;

	++array;
	
	return array;
}

inline void __impl__array_close(void** ptr)
{
	if (*ptr == NULL) return;
	
	DynamicArrayHeader* array = (DynamicArrayHeader*)*ptr;
	--array;
	
	memory_free(array);
	*ptr = NULL;
}

inline void __impl__array_reset(void** ptr)
{
	if (*ptr == NULL) return;
	
	DynamicArrayHeader* array = (DynamicArrayHeader*)*ptr;
	--array;

	array->size = 0u;
}

inline void* __impl__array_add(void** ptr, u32 stride, u32 line, const char* file)
{
	DynamicArrayHeader* array = (DynamicArrayHeader*)*ptr;
	--array;
	
	if (array->size == array->capacity) {
		
		u32 new_capacity = (u32)((f32)(array->capacity + 1) * array->scale_factor);
		u32 bytes = sizeof(DynamicArrayHeader) + (new_capacity * stride);
		
		DynamicArrayHeader* new_array = (DynamicArrayHeader*)__impl__memory_allocate(bytes, line, file);

		memory_copy(new_array, array, sizeof(DynamicArrayHeader) + (array->size * stride));
		memory_free(array);

		*ptr = (u8*)(new_array) + sizeof(DynamicArrayHeader);
		array = new_array;

		array->capacity = new_capacity;
	}

	void* res = (u8*)(*ptr) + (array->size * stride);
	++array->size;
	return res;
}

inline void __impl__array_push(void** ptr, u32 stride, const void* data, u32 line, const char* file)
{
	void* obj = __impl__array_add(ptr, stride, line, file);
	memory_copy(obj, data, stride);
}

inline void __impl__array_pop(void** ptr)
{
	if (*ptr == NULL) return;
	
	DynamicArrayHeader* array = (DynamicArrayHeader*)*ptr;
	--array;

	assert(array->size != 0);
	--array->size;
}

inline u32 __impl__array_size(void** ptr)
{
	if (*ptr == NULL) return 0u;
	
	DynamicArrayHeader* array = (DynamicArrayHeader*)*ptr;
	--array;

	return array->size;
}

inline b8 __impl__array_empty(void** ptr)
{
	if (*ptr == NULL) return 0u;
	
	DynamicArrayHeader* array = (DynamicArrayHeader*)*ptr;
	--array;

	return array->size == 0;
}

#define array_init(T, initial_capacity, scale_factor) (T*)__impl__array_init(sizeof(T), initial_capacity, scale_factor, __LINE__, __FILE__)
#define array_close(ptr) __impl__array_close((void**)&(ptr))
#define array_reset(ptr) __impl__array_reset((void**)&(ptr))

#define array_add(ptr) __impl__array_add((void**)&(ptr), sizeof(*ptr), __LINE__, __FILE__)	
#define array_push(ptr, obj) __impl__array_push((void**)&(ptr), sizeof(*ptr), &obj, __LINE__, __FILE__)
#define array_pop(ptr) __impl__array_pop((void**)&(ptr))

#define array_size(ptr) __impl__array_size((void**)&(ptr))
#define array_empty(ptr) __impl__array_empty((void**)&(ptr))

typedef struct {
	u8* data;
	u32 size;
	u32 capacity;
	f32 scale_factor;
} Buffer;

inline Buffer buffer_init(f32 scale_factor)
{
	Buffer buffer;
	buffer.data = NULL;
	buffer.size = 0u;
	buffer.capacity = 0u;
	buffer.scale_factor = SV_MAX(scale_factor, 1.f);
	return buffer;
}

inline void __impl__buffer_resize(Buffer* buffer, u32 size, u32 line, const char* filepath)
{
	if (buffer->size < size) {

		if (size > buffer->capacity) {

			u8* new_data = (u8*)__impl__memory_allocate(size, line, filepath);
			buffer->capacity = size;

			if (buffer->size) {
				memory_copy(new_data, buffer->data, buffer->size);
				memory_free(buffer->data);
			}

			buffer->data = new_data;
		}
	}
	else if (buffer->size > size) {
		memory_zero(buffer->data + size, buffer->size - size);
	}

	buffer->size = size;
}

inline void buffer_close(Buffer* buffer)
{
	if (buffer->data) {
		memory_free(buffer->data);
	}
}

inline void buffer_reset(Buffer* buffer)
{
	if (buffer->size) {
		memory_zero(buffer->data, buffer->size);
	}
	
	buffer->size = 0u;
}

inline void __impl__buffer_write_back(Buffer* buffer, const void* data, u32 size, u32 line, const char* file)
{
	u32 required = buffer->size + size;
	
	if (buffer->capacity < required) {

		u32 new_capacity = (u32)((f32)required * buffer->scale_factor);
		u8* new_data = (u8*)__impl__memory_allocate(new_capacity, line, file);

		if (buffer->size) {
			memory_copy(new_data, buffer->data, buffer->size);
			memory_free(buffer->data);
		}

		buffer->data = new_data;
		buffer->capacity = new_capacity;
	}

	memory_copy(buffer->data + buffer->size, data, size);
	buffer->size += size;
}

#define buffer_resize(buffer, size) __impl__buffer_resize(buffer, size, __LINE__, __FILE__)
#define buffer_write_back(buffer, data, size) __impl__buffer_write_back(buffer, data, size, __LINE__, __FILE__)

typedef struct {
	u8* instances;
	u32 size;
	u32 begin_count;
} InstanceAllocatorPool;

typedef struct {
	InstanceAllocatorPool* pools;
	u32 pool_count;
	u32 instance_size;
	u32 pool_size;
} InstanceAllocator;

inline InstanceAllocator instance_allocator_init(u32 instance_size, u32 pool_size)
{
	InstanceAllocator alloc;
	alloc.pools = NULL;
	alloc.pool_count = 0u;
	alloc.instance_size = SV_MAX(instance_size, sizeof(u32));
	alloc.pool_size = pool_size;
	return alloc;
}

inline void instance_allocator_close(InstanceAllocator* alloc)
{
	foreach(i, alloc->pool_count) {

		void* ptr = alloc->pools[i].instances;
		
		if (ptr != NULL)
			memory_free(ptr);
	}

	if (alloc->pools != NULL)
		memory_free(alloc->pools);

	alloc->pools = NULL;
	alloc->pool_count = 0u;
}

inline void* __impl__instance_allocator_create(InstanceAllocator* alloc, u32 line, const char* file)
{
	// Find pool
	InstanceAllocatorPool* pool = NULL;

	foreach(i, alloc->pool_count) {

		if (alloc->pools[i].begin_count != u32_max) {
			pool = alloc->pools + i;
			break;
		}
	}

	if (pool == NULL) {
		foreach(i, alloc->pool_count) {
			
			if (alloc->pools[i].size < alloc->pool_size) {
				pool = alloc->pools + i;
				break;
			}
		}
	}
	
	if (pool == NULL) {

		u32 new_pool_count = alloc->pool_count + 4;
		InstanceAllocatorPool* new_pools = (InstanceAllocatorPool*)__impl__memory_allocate(new_pool_count * sizeof(InstanceAllocatorPool), line, file);
		memory_copy(new_pools, alloc->pools, alloc->pool_count * sizeof(InstanceAllocatorPool));
		alloc->pools = new_pools;

		pool = alloc->pools + alloc->pool_count;

		for (u32 i = alloc->pool_count; i < new_pool_count; ++i) {

			alloc->pools[i].instances = NULL;
			alloc->pools[i].size = 0u;
			alloc->pools[i].begin_count = u32_max;
		}
		
		alloc->pool_count = new_pool_count;
	}

	if (pool->instances == NULL)
		pool->instances = (u8*)__impl__memory_allocate(alloc->pool_size * alloc->instance_size, line, file);

	void* ptr;

	if (pool->begin_count != u32_max) {

		ptr = pool->instances + pool->begin_count * alloc->instance_size;
		
		// Change nextCount
		u32 next = *(u32*)ptr;
		
		if (next == u32_max)
			pool->begin_count = u32_max;
		else
			pool->begin_count += next;
	}
	else {
		ptr = pool->instances + pool->size * alloc->instance_size;
		++pool->size;
	}

	memory_zero(ptr, alloc->instance_size);

	return ptr;
}

inline u32* _instance_allocator_find_last_count(InstanceAllocator* alloc, void* ptr, InstanceAllocatorPool* pool)
{
	u32* next_count;

	if (pool->begin_count == u32_max) return &pool->begin_count;

	u8* next = pool->instances + (pool->begin_count * alloc->instance_size);
	u8* end = pool->instances + (pool->size * alloc->instance_size);

	if (next >= ptr) next_count = &pool->begin_count;
	else {
		u8* last = next;

		while (next != end) {

			u32 next_value = *(u32*)last;
			if (next_value == u32_max) break;

			next += next_value * alloc->instance_size;
			if (next > ptr) break;

			last = next;
		}
		next_count = (u32*)last;
	}

	return next_count;
}

inline void instance_allocator_destroy(InstanceAllocator* alloc, void* ptr_)
{
	u8* ptr = (u8*)ptr_;

	// Find pool
	InstanceAllocatorPool* pool = NULL;

	foreach(i, alloc->pool_count) {

		InstanceAllocatorPool* p = alloc->pools + i;
		
		if (p->instances != NULL && p->instances <= ptr && p->instances + (p->size * alloc->instance_size) > ptr) {

			pool = p;
			break;
		}
	}

	if (pool == NULL)
		return;

	// Remove from list
	if (ptr == pool->instances + (pool->size - 1u) * alloc->instance_size) {
		--pool->size;
	}
	else {
		// Update next count
		u32* next_count = _instance_allocator_find_last_count(alloc, ptr, pool);

		u32 distance;

		if (next_count == &pool->begin_count) {
			distance = (u32)((ptr - pool->instances) / alloc->instance_size);
		}
		else distance = (u32)((ptr - (u8*)next_count) / alloc->instance_size);

		u32* free_instance = (u32*)ptr;

		if (*next_count == u32_max)
			*free_instance = u32_max;
		else
			*free_instance = *next_count - distance;

		*next_count = distance;
	}
}

inline u32 instance_allocator_size(InstanceAllocator* alloc)
{
	// TODO:
	return 0;
}

#define instance_allocator_create(alloc) __impl__instance_allocator_create(alloc, __LINE__, __FILE__)

typedef struct {
	InstanceAllocator* allocator;
	void* ptr;
	u8* _next;
	u32 _pool;
	b8 has_next;
} InstanceIterator;

inline void instance_iterator_next(InstanceIterator* it)
{
	InstanceAllocator* alloc = it->allocator;
	u8* ptr = (u8*)it->ptr;

	it->has_next = TRUE;

	if (ptr == NULL) {
		
		while (ptr == NULL) {

			if (alloc->pool_count <= it->_pool) {
				it->has_next = FALSE;
				return;
			}
		
			InstanceAllocatorPool* pool = alloc->pools + it->_pool;

			if (pool->size) {

				ptr = pool->instances;

				if (pool->begin_count == u32_max) {
					it->_next = (u8*)&pool->begin_count;
				}
				else {
					it->_next = pool->instances + pool->begin_count * alloc->instance_size;
				}
			}
			else ++it->_pool;
		}
	}
	else ptr += alloc->instance_size;

	while (ptr == it->_next) {
		
		u32* next_count = (u32*)it->_next;
		if (*next_count == u32_max) {
			ptr += alloc->instance_size;
			break;
		}
		it->_next += *next_count * alloc->instance_size;
		ptr += alloc->instance_size;
	}

	InstanceAllocatorPool* pool = alloc->pools + it->_pool;
	if (ptr == pool->instances + pool->size * alloc->instance_size) {

		it->ptr = NULL;
		++it->_pool;
		instance_iterator_next(it);
	}
	else it->ptr = ptr;
}

inline InstanceIterator instance_iterator_begin(InstanceAllocator* allocator)
{
	InstanceIterator it;
	it.allocator = allocator;
	it._pool = 0u;
	it.ptr = NULL;
	it._next = NULL;

	instance_iterator_next(&it);
	
	return it;
}

#define foreach_instance(it, allocator) for (InstanceIterator it = instance_iterator_begin(allocator); it.has_next; instance_iterator_next(&it))

typedef struct {
	char* data;
	u32 size;
	u32 capacity;
	f32 scale_factor;
} DynamicString;

inline DynamicString __impl__dynamic_string_init(const char* init_string, f32 scale_factor, u32 line, const char* file)
{
	DynamicString str;
	str.scale_factor = SV_MIN(scale_factor, 1.f);

	init_string = string_validate(init_string);

	if (init_string[0] == '\0') {
		str.data = "";
		str.size = 0u;
		str.capacity = 0u;
	}
	else {
		u32 size = string_size(init_string);
		
		str.capacity = (u32)((f32)size * str.scale_factor);
		str.size = size;
		str.data = (char*)__impl__memory_allocate(str.capacity + 1, line, file);

		memory_copy(str.data, init_string, str.size + 1);
	}
	
	return str;
}

inline void dynamic_string_close(DynamicString* str)
{
	if (str->data) {
		memory_free(str->data);
	}
}

inline void __impl__dynamic_string_resize(DynamicString* str, u32 size, u32 line, const char* file)
{
	if (str->size < size) {

		if (size > str->capacity) {

			u8* new_data = (u8*)__impl__memory_allocate(size + 1, line, file);
			str->capacity = size;

			if (str->size) {
				memory_copy(new_data, str->data, str->size + 1);
				memory_free(str->data);
			}

			str->data = (char*)new_data;
		}

		str->size = size;
	}
	else if (str->size > size) {
		memory_zero(str->data + size, str->size - size);
		str->size = size;
	}
}

#define dynamic_string_init(init_string, scale_factor) __impl__dynamic_string_init(init_string, scale_factor, __LINE__, __FILE__)
#define dynamic_string_resize(str, size) __impl__dynamic_string_resize(str, size, __LINE__, __FILE__)

typedef struct {
	u32 todo;
} HashTable;

#ifdef __cplusplus
}
#endif
