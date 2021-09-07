#include "Hosebase/serialize.h"

#define STBI_ASSERT(x) assert(x)
#define STBI_MALLOC(size) memory_allocate(size)
#define STBI_REALLOC(ptr, size) realloc(ptr, size)
#define STBI_FREE(ptr) memory_free(ptr)
#define STB_IMAGE_IMPLEMENTATION

#include "Hosebase/external/stbi_lib.h"

b8 load_image(const char* filepath_, void** pdata, u32* width, u32* height)
{
	char filepath[FILE_PATH_SIZE];
	filepath_resolve(filepath, filepath_);
	
	int w = 0, h = 0, bits = 0;
	void* data = stbi_load(filepath, &w, &h, &bits, 4);

	* pdata = NULL;
	*width = w;
	*height = h;

	if (!data) return FALSE;
	*pdata = data;
	return TRUE;
}

#define BIN_PATH_SIZE 101
    
inline void bin_filepath(char* buf, u64 hash, b8 system)
{
	if (system)
		sprintf(buf, "$system/cache/%llu.bin", hash);
	else
		sprintf(buf, "bin/%llu.bin", hash);
}

b8 bin_read(u64 hash, Buffer* data, b8 system)
{
	char filepath[BIN_PATH_SIZE];
	bin_filepath(filepath, hash, system);
	return file_read_binary(filepath, data);
}
    
/*b8 bin_read(u64 hash, Deserializer& deserializer, b8 system)
  {
  char filepath[BIN_PATH_SIZE];
  bin_filepath(filepath, hash, system);
  return deserialize_begin(deserializer, filepath);
  }*/

b8 bin_write(u64 hash, const void* data, size_t size, b8 system)
{
	char filepath[BIN_PATH_SIZE];
	bin_filepath(filepath, hash, system);
	return file_write_binary(filepath, (u8*)data, size, FALSE, TRUE);
}
    
/*b8 bin_write(u64 hash, Serializer& serializer, b8 system)
  {
  char filepath[BIN_PATH_SIZE];
  bin_filepath(filepath, hash, system);
  return serialize_end(serializer, filepath);
  }*/
