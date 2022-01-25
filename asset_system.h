#pragma once

#include "serialize.h"

// That's only for assets attached to files
typedef enum {
	AssetPriority_RightNow,
	AssetPriority_KeepItLoading,
	AssetPriority_GetIfExists,
} AssetPriority;

Asset asset_load_from_file(const char* filepath, AssetPriority priority);

void asset_unload(Asset* asset);

void asset_increment(Asset asset);
void asset_decrement(Asset asset);

void*       asset_get(Asset asset);
void*       asset_get_ptr(Asset asset);
const char* asset_filepath(Asset asset);
const char* asset_type(Asset asset);

void serialize_asset(Serializer* s, Asset asset);
void deserialize_asset(Deserializer* s, Asset* asset, AssetPriority priority);

typedef b8(*AssetLoadFileFn)(void* asset, const char* filepath);
typedef b8(*AssetReloadFileFn)(void* asset, const char* filepath);
typedef void(*AssetFreeFn)(void* asset);

typedef struct {

	const char*       name;
	u32		          asset_size;
	const char**      extensions;
	u32		          extension_count;
	AssetLoadFileFn	  load_file_fn;
	AssetReloadFileFn reload_file_fn;
	AssetFreeFn	      free_fn;
	f32		          unused_time;

} AssetTypeDesc;

b8 asset_register_type(const AssetTypeDesc* desc);

void asset_free_unused();

b8 _asset_initialize(b8 hot_reloading);
void _asset_close();
void _asset_update();