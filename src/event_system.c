#include "Hosebase/event_system.h"

#define EVENT_TABLE_SIZE 5000
#define EVENT_REGISTER_MAX 50

typedef struct EventType EventType;

typedef struct {
	EventFn fn;
} EventRegister;

struct EventType {

	EventRegister registers[EVENT_REGISTER_MAX];
	u32 register_count;

	// Used in hash table
	u64 hash; // or id
	EventType* next;
};

typedef struct {

	EventType type_table[EVENT_TABLE_SIZE];

} EventSystemData;

static EventSystemData* event;

b8 _event_initialize()
{
	event = memory_allocate(sizeof(EventSystemData));

	return TRUE;
}

void free_entry(EventType* type)
{
	if (type->next) {
		free_entry(type->next);
		memory_free(type->next);
	}
}

void _event_close()
{
	if (event) {

		foreach(i, EVENT_TABLE_SIZE) {

			EventType* type = event->type_table + i;
			free_entry(type);
		}

		memory_free(event);
	}
}

inline b8 find_in_table(u64 hash, EventType** _type, EventType** _parent, b8 create)
{
	u32 index = hash % EVENT_TABLE_SIZE;

	EventType* type = event->type_table + index;
	EventType* parent = NULL;

	while (type->hash != hash) {

		parent = type;
		type = type->next;
		if (type == NULL) {

			if (create) {
				type = memory_allocate(sizeof(EventType));
				parent->next = type;

				type->hash = hash;
			}
			else break;
		}
	}

	if (type == NULL) return FALSE;
	else {

		*_type = type;
		*_parent = parent;
	}

	return TRUE;
}

inline void erase_entry(EventType* type, EventType* parent)
{
	parent->next = type->next;

	if (parent == NULL) {
		memory_zero(type, sizeof(EventType));
	}
	else {
		memory_free(type);
	}
}

b8 event_register(u64 handle, const char* name, EventFn fn)
{
	EventType* type;
	EventType* parent;

	u64 hash = hash_combine(handle, hash_string(name));
	find_in_table(hash, &type, &parent, TRUE);

	if (type->register_count >= EVENT_REGISTER_MAX) {

		SV_LOG_ERROR("Can't register more than %u events\n", EVENT_REGISTER_MAX);
		return FALSE;
	}
	else {

		b8 found = FALSE;

		foreach(i, type->register_count) {

			if (type->registers[i].fn == fn) {
				found = TRUE;
				break;
			}
		}

		if (found)
			return FALSE;

		EventRegister reg;
		reg.fn = fn;

		type->registers[type->register_count++] = reg;
		return TRUE;
	}
}

b8 event_unregister(u64 handle, const char* name, EventFn fn)
{
	EventType* type;
	EventType* parent;

	u64 hash = hash_combine(handle, hash_string(name));
	if (!find_in_table(hash, &type, &parent, FALSE))
		return FALSE;

	b8 found = FALSE;

	foreach(i, type->register_count) {

		if (type->registers[i].fn == fn) {

			--type->register_count;
			for (u32 j = i; j < type->register_count; ++j) {

				type->registers[j + 0] = type->registers[j + 1];
			}

			found = TRUE;
			break;
		}
	}

	if (type->register_count == 0) {

		erase_entry(type, parent);
	}

	return found;
}

b8 event_unregister_all(u64 handle, const char* name)
{
	EventType* type;
	EventType* parent;

	u64 hash = hash_combine(handle, hash_string(name));
	if (!find_in_table(hash, &type, &parent, FALSE))
		return FALSE;

	erase_entry(type, parent);
	return TRUE;
}

void event_dispatch(u64 handle, const char* name, void* data)
{
	EventType* type;
	EventType* parent;

	u64 hash = hash_combine(handle, hash_string(name));
	if (!find_in_table(hash, &type, &parent, FALSE))
		return;

	foreach(i, type->register_count) {

		EventRegister reg = type->registers[i];
		reg.fn(data);
	}
}