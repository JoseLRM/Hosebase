#include "asset_system.h"

#define EXTENSION_MAX 10
#define ASSET_TYPE_MAX 20
#define ASSET_TABLE_SIZE 4000

#define AssetFlag_Valid SV_BIT(0)
#define AssetFlag_FromFile SV_BIT(1)

typedef struct {
	u64 hash;
	f64 last_update;
	Date last_file_update; // Used in hot reloading
	u32 flags;
	u32 next; // This is used in the hash table and should be initialized with all ones at the begining
	volatile u32 reference_counter;

	// TODO: Move this to a separate buffer
	char text[FILE_PATH_SIZE]; // That represent the filepath if is attached to a file or a name
} AssetHeader;

typedef struct {

	char name[NAME_SIZE];
	u32 asset_size;

	char extensions[NAME_SIZE][EXTENSION_MAX];
	u32 extension_count;

	AssetLoadFileFn	load_file_fn;
	AssetReloadFileFn reload_file_fn;
	AssetFreeFn	free_fn;
	f32	unused_time;

	// Assets
	u8* asset_memory;
	u32 asset_count;
	u32 asset_capacity;
	u32 asset_free_count;

	// Asset table
	u32 asset_table[ASSET_TABLE_SIZE]; // At the beggining this table should be initialized with all ones

} AssetType;

typedef struct {
	AssetType types[ASSET_TYPE_MAX];
	u32 type_count;

	b8 hot_reloading;
} AssetSystemData;

static AssetSystemData* sys;

inline Asset asset_handle(u32 index, AssetType* type)
{
	u64 type_index = type - sys->types + 1;
	return (u64)index | (type_index << 32);
}

inline void asset_decompose(Asset asset, u32* index, AssetType** type)
{
	if (index)* index = asset & 0xFFFFFFFF;
	if (type) {
		u32 type_index = (asset >> 32) & 0xFFFFFFFF;
		if (type_index == 0) {
			*type = NULL;
		}
		else {
			*type = sys->types + (type_index - 1);
		}
	}
}

inline AssetHeader* asset_decompose_ptr(Asset asset)
{
	u32 index;
	AssetType* type;
	asset_decompose(asset, &index, &type);

	if (type == NULL)
		return NULL;

	return (AssetHeader*)(type->asset_memory + (index * (sizeof(AssetHeader) + type->asset_size)));
}

inline u64 compute_asset_filepath_hash(const char* filepath)
{
	u64 hash = hash_string(filepath);
	return hash_combine(hash, 0x93aff8a7934);
}

static Asset find_asset_in_table(AssetType* type, u64 hash)
{
	u32 asset_stride = sizeof(AssetHeader) + type->asset_size;

	u32 index = type->asset_table[hash % ASSET_TABLE_SIZE];

	if (index != u32_max) {
		AssetHeader* next = (AssetHeader*)(type->asset_memory + (index * asset_stride));

		while (next->hash != hash) {

			if (next->next == u32_max) {
				next = NULL;
				break;
			}
			else {
				next = (AssetHeader*)(type->asset_memory + (next->next * asset_stride));
			}
		}

		if (next) {
			u32 index = ((u8*)next - type->asset_memory) / asset_stride;

			return asset_handle(index, type);
		}
	}

	return 0;
}

static void store_asset_in_table(Asset asset, u64 hash)
{
	u32 asset_index;
	AssetType* type;
	asset_decompose(asset, &asset_index, &type);

	if (type == NULL)
		return;

	u32 asset_stride = sizeof(AssetHeader) + type->asset_size;
	u32 index = type->asset_table[hash % ASSET_TABLE_SIZE];

	if (index != u32_max) {
		AssetHeader* next = (AssetHeader*)(type->asset_memory + (index * asset_stride));

		while (next->hash != hash) {

			if (next->next == u32_max) {
				next->next = asset_index;
			}
			
			next = (AssetHeader*)(type->asset_memory + (next->next * asset_stride));
		}
	}
	else type->asset_table[hash % ASSET_TABLE_SIZE] = asset_index;
}

static void remove_asset_in_table(AssetType* type, u64 hash)
{
	u32 asset_stride = sizeof(AssetHeader) + type->asset_size;
	u32 index = type->asset_table[hash % ASSET_TABLE_SIZE];

	if (index != u32_max) {
		AssetHeader* next = (AssetHeader*)(type->asset_memory + (index * asset_stride));
		AssetHeader* parent = NULL;

		while (next->hash != hash) {

			if (next->next == u32_max) {
				break;
			}

			parent = next;
			next = (AssetHeader*)(type->asset_memory + (next->next * asset_stride));
		}

		if (next->hash == hash) {

			if (parent) {
				parent->next = next->next;
			}
			else {
				type->asset_table[hash % ASSET_TABLE_SIZE] = u32_max;
			}
		}
	}
}

static Asset allocate_asset(AssetType* type, u64 hash)
{
	u32 asset_index = u32_max;

	u32 asset_stride = type->asset_size + sizeof(AssetHeader);

	// Reserve memory
	{
		if (type->asset_free_count) {

			foreach(i, type->asset_count) {

				AssetHeader* asset = (AssetHeader*)(type->asset_memory + (i * asset_stride));
				if (!(asset->flags & AssetFlag_Valid)) {
					asset_index = i;
					break;
				}
			}

			assert(asset_index != u32_max);
			type->asset_free_count--;
		}

		if (asset_index == u32_max) {
			array_prepare(&type->asset_memory, &type->asset_count, &type->asset_capacity, type->asset_capacity + 100, 1, asset_stride);
			asset_index = type->asset_count++;
		}
	}

	// Initialize
	{
		AssetHeader* asset = (AssetHeader*)(type->asset_memory + (asset_index * asset_stride));
		asset->flags |= AssetFlag_Valid;
		asset->hash = hash;
		asset->next = u32_max;
		asset->last_update = timer_now();
	}

	Asset asset = asset_handle(asset_index, type);

	store_asset_in_table(asset, hash);

	return asset;
}

static void free_asset(Asset asset)
{
	u32 asset_index;
	AssetType* type;
	asset_decompose(asset, &asset_index, &type);

	if (type == NULL)
		return;

	u32 asset_stride = sizeof(AssetHeader) + type->asset_size;

	AssetHeader* header = (AssetHeader*)(type->asset_memory + (asset_index * asset_stride));
	remove_asset_in_table(type, header->hash);

	memory_zero(header, asset_stride);

	if (type->asset_count - 1 == asset_index) {

		type->asset_count--;
	}
	else {
		
		type->asset_free_count++;
	}
}

static AssetType* find_asset_type(const char* name) 
{
	foreach(i, sys->type_count) {
		AssetType* type = sys->types + i;

		if (string_equals(type->name, name)) {
			return type;
		}
	}
	return NULL;
}

static AssetType* find_asset_type_from_filepath(const char* filepath)
{
	const char* extension;

	// Compute extension
	{
		extension = filepath_extension(filepath);

		if (extension == NULL)
			return NULL;

		if (extension[0] == '.')
			++extension;

		if (extension[0] == '\0')
			return NULL;
	}

	foreach(i, sys->type_count) {
		AssetType* type = sys->types + i;

		foreach(j, type->extension_count) {

			if (string_equals(type->extensions[j], extension)) {
				return type;
			}
		}
	}

	return NULL;
}

b8 _asset_initialize(b8 hot_reloading)
{
	sys = memory_allocate(sizeof(AssetSystemData));

	sys->hot_reloading = hot_reloading;

	return TRUE;
}

void _asset_close()
{
	if (sys) {

		foreach(i, sys->type_count) {

			AssetType* type = sys->types + i;
			
			foreach(i, type->asset_count) {

				AssetHeader* asset = (AssetHeader*)(type->asset_memory + (i * (sizeof(AssetHeader) + type->asset_size)));

				if (asset->flags & AssetFlag_Valid) {

					if (asset->flags & AssetFlag_FromFile) {
						SV_LOG_ERROR("Asset '%s' not freed from file '%s'\n", type->name, asset->text);
					}
					else {
						SV_LOG_ERROR("Asset '%s' not freed\n", type->name);
					}
				}
			}

			if (type->asset_memory)
				memory_free(type->asset_memory);
		}

		memory_free(sys);
	}
}

void _asset_update()
{
	u32 frame = core.frame_count;

	const u32 update_rate = 5;

	// Reduce the updates
	if (frame % update_rate != 0)
		return;

	AssetType* type = sys->types + ((frame / update_rate) % sys->type_count);

	f64 now = timer_now();

	foreach(i, type->asset_count) {

		AssetHeader* asset = (AssetHeader*)(type->asset_memory + (i * (sizeof(AssetHeader) + type->asset_size)));

		if (asset->flags & AssetFlag_Valid) {

			if (asset->reference_counter == 0) {

				if (now - asset->last_update > type->unused_time) {

					type->free_fn(asset + 1);

					if (asset->flags & AssetFlag_FromFile) {
						SV_LOG_INFO("Asset '%s' freed from file '%s'\n", type->name, asset->text);
					}
					else {
						SV_LOG_INFO("Asset '%s' freed\n", type->name);
					}

					free_asset(asset_handle(i, type));
				}
			}
			else {
				asset->last_update = now;
			}
		}
	}

	// Hot reloading
	if (sys->hot_reloading) {

		foreach(i, type->asset_count) {

			AssetHeader* asset = (AssetHeader*)(type->asset_memory + (i * (sizeof(AssetHeader) + type->asset_size)));

			u64 flags = AssetFlag_FromFile | AssetFlag_Valid;

			if (asset->flags & flags == flags) {

				const char* filepath = asset->text;

				Date last_update;

				if (file_date(filepath, NULL, &last_update, NULL)) {

					if (!date_equals(last_update, asset->last_file_update)) {
						
						asset->last_file_update = last_update;

						if (type->reload_file_fn(asset + 1, filepath)) {
							SV_LOG_INFO("Asset '%s' reloaded from file '%s'\n", type->name, filepath);
						}
						else {
							SV_LOG_ERROR("Asset '%s' can't be reloaded from file '%s'\n", type->name, filepath);
							free_asset(asset_handle(i, type));
						}
					}
				}
			}
		}
	}
}

void asset_free_unused()
{
	foreach(i, sys->type_count) {

		AssetType* type = sys->types + i;

		foreach(i, type->asset_count) {

			AssetHeader* asset = (AssetHeader*)(type->asset_memory + (i * (sizeof(AssetHeader) + type->asset_size)));

			if (asset->flags & AssetFlag_Valid && asset->reference_counter == 0) {

				type->free_fn(asset + 1);

				if (asset->flags & AssetFlag_FromFile) {
					SV_LOG_ERROR("Asset '%s' freed from file '%s'\n", type->name, asset->text);
				}
				else {
					SV_LOG_ERROR("Asset '%s' freed\n", type->name);
				}
			}
		}
	}
}

b8 asset_register_type(const AssetTypeDesc* desc)
{
	if (sys->type_count >= ASSET_TYPE_MAX) {
		SV_LOG_ERROR("The asset type limit is %u\n", ASSET_TYPE_MAX);
		return FALSE;
	}

	if (desc->extension_count == 0) {
		SV_LOG_ERROR("The asset type needs almost one extension\n");
		return FALSE;
	}

	// Check for duplicated extensions
	{
		u32 duplicated_index = u32_max;

		foreach(i, sys->type_count) {

			AssetType* type = sys->types + i;
			foreach(j, type->extension_count) {
				
				const char* e0 = type->extensions[j];
				foreach(w, desc->extension_count) {

					const char* e1 = desc->extensions[w];
					if (string_equals(e0, e1)) {
						duplicated_index = w;
						break;
					}
				}
			}
		}

		if (duplicated_index != u32_max) {
			SV_LOG_ERROR("Extension '%s' duplicated in asset\n", desc->extensions[duplicated_index]);
			return FALSE;
		}
	}

	// Check for duplicated name
	{
		if (find_asset_type(desc->name) != NULL) {
			SV_LOG_ERROR("Repeated asset type name '%s'\n", desc->name);
			return FALSE;
		}
	}

	AssetType* type = sys->types + sys->type_count++;

	string_copy(type->name, desc->name, NAME_SIZE);
	type->asset_size = desc->asset_size;

	foreach(i, desc->extension_count) {

		string_copy(type->extensions[i], desc->extensions[i], NAME_SIZE);
	}
	type->extension_count = desc->extension_count;

	type->load_file_fn = desc->load_file_fn;
	type->reload_file_fn = desc->reload_file_fn;
	type->free_fn = desc->free_fn;
	type->unused_time = desc->unused_time;

	foreach(i, ASSET_TABLE_SIZE) {
		type->asset_table[i] = u32_max;
	}

	return TRUE;
}

Asset asset_load_from_file(const char* filepath, AssetPriority priority)
{
	AssetType* type = find_asset_type_from_filepath(filepath);

	u64 hash = compute_asset_filepath_hash(filepath);

	Asset asset_handle = find_asset_in_table(type, hash);

	if (asset_handle) {
		asset_increment(asset_handle);

		AssetHeader* asset = asset_decompose_ptr(asset_handle);
		if (asset)
			asset->last_update = timer_now();
		return asset_handle;
	}
	else {

		asset_handle = allocate_asset(type, hash);

		u32 asset_index;
		asset_decompose(asset_handle, &asset_index, NULL);

		AssetHeader* asset = (AssetHeader*)(type->asset_memory + (asset_index * (sizeof(AssetHeader) + type->asset_size)));
		
		// Init asset
		if (!type->load_file_fn(asset + 1, filepath)) {

			SV_LOG_ERROR("Can't load the asset '%s' from '%s'\n", type->name, filepath);
			free_asset(asset_handle);
			return 0;
		}

		asset->flags |= AssetFlag_FromFile;
		string_copy(asset->text, filepath, FILE_PATH_SIZE);
		asset_increment(asset_handle);

		file_date(filepath, NULL, &asset->last_file_update, NULL);

		SV_LOG_INFO("Asset '%s' loaded from '%s'\n", type->name, filepath);

		return asset_handle;
	}
}

void asset_unload(Asset* passet)
{
	Asset asset = *passet;
	*passet = 0;
	asset_decrement(asset);
}

void asset_increment(Asset asset)
{
	AssetHeader* a = asset_decompose_ptr(asset);
	if (a) {
		interlock_increment_u32(&a->reference_counter);
	}
}

void asset_decrement(Asset asset)
{
	AssetHeader* a = asset_decompose_ptr(asset);
	if (a) {
		interlock_decrement_u32(&a->reference_counter);
	}
}

void* asset_get(Asset asset)
{
	AssetHeader* a = asset_decompose_ptr(asset);
	if (a) {
		return a + 1;
	}
	return NULL;
}

const char* asset_filepath(Asset asset)
{
	AssetHeader* a = asset_decompose_ptr(asset);
	if (a && a->flags & AssetFlag_FromFile) {
		return a->text;
	}
	return NULL;
}

const char* asset_type(Asset asset)
{
	AssetType* type;
	asset_decompose(asset, NULL, &type);
	if (type == NULL)
		return NULL;
	return type->name;
}

void serialize_asset(Serializer* s, Asset asset)
{
	// TODO
}

void deserialize_asset(Deserializer* s, Asset* asset, AssetPriority priority)
{
	// TODO
}