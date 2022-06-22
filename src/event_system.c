#include "Hosebase/event_system.h"

#include "Hosebase/memory_manager.h"

#define EVENT_TABLE_SIZE 1000

typedef struct {
	EventFn fn;
	u64 handle;
} EventRegister;

typedef struct {
	char name[NAME_SIZE];
	EventRegister registers[400];
	u32 register_count;
	HashTableEntry entry;
} EventType;

typedef struct {

	EventType event_table[EVENT_TABLE_SIZE];

} EventSystem;

static EventSystem* event_system;

static EventType* _event_type_get(u64 hash, b8 create, b8* created)
{
	return hashtable_get(hash, event_system->event_table, sizeof(EventType), EVENT_TABLE_SIZE, create, created);
}

u64 event_compute_handle(const char* system_name, u64 handle)
{
	return hash_combine(handle, hash_string(string_validate(system_name)));
}

b8 event_register(u64 handle, const char* name, EventFn fn)
{
	if (handle == 0)
		handle = 0x29898665345ULL;
	
	EventRegister reg;
	reg.fn = fn;
	reg.handle = handle;
	
	u64 hash = hash_string(name);

	EventType* type = _event_type_get(hash, TRUE, NULL);

	if (type->register_count >= SV_ARRAY_SIZE(type->registers))
	{
		// TODO: Use auxiliar dynamic buffer
		assert_title(FALSE, "Event register overflow!!");
	}
	else
	{
		type->registers[type->register_count++] = reg;
	}

	return TRUE;
}

void event_unregister_handle(u64 handle)
{
	// TODO: 
}

void event_dispatch(u64 handle, const char* name, void* data)
{
	u64 hash = hash_string(name);
	EventType* type = _event_type_get(hash, FALSE, NULL);

	if (type != NULL)
	{
		foreach(i, type->register_count)
		{
			EventRegister* reg = type->registers + i;
			
			if (handle == 0 || reg->handle == handle)
			{
				reg->fn(data);
			}
		}
	}
}

b8 _event_initialize()
{
	event_system = memory_allocate(sizeof(EventSystem));
	return TRUE;
}

void _event_close()
{
	if (event_system != NULL)
	{
		hashtable_free(event_system->event_table, sizeof(EventType), EVENT_TABLE_SIZE);

		memory_free(event_system);
	}
}