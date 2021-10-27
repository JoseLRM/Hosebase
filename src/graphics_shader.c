#include "Hosebase/defines.h"

#if SV_GRAPHICS

#include "graphics_internal.h"

b8 graphics_shader_initialize()
{
	return TRUE;
}

b8 graphics_shader_close()
{
	return TRUE;
}

inline void compute_random_path(char* str)
{
	// TODO: Thread safe
	static u32 seed = 0x734FC7A7832;
	u32 random = math_random_u32(seed);
	seed += 0x434F32A;

	char random_str[20];
	string_from_u32(random_str, random);

	string_copy(str, "bin/", FILE_PATH_SIZE);
	string_append(str, random_str, FILE_PATH_SIZE);
}

b8 graphics_shader_compile_string(const ShaderCompileDesc* desc, const char* str, u32 size, Buffer* data)
{
	char filepath[FILE_PATH_SIZE];
	compute_random_path(filepath);
	
	b8 res = file_write_text(filepath, str, size, FALSE, TRUE);
	if (!res) return FALSE;

	res = graphics_shader_compile_file(desc, filepath, data);

	if (!res) {
		file_remove(filepath);
		return FALSE;
	}

	res = file_remove(filepath);

	return res;
}

#define BAT_SIZE 701

inline void append_bat(char* bat, u32* offset, const char* src)
{
	u32 size = string_size(src);
	if (size + *offset > BAT_SIZE) {
		SV_LOG_ERROR("Shader compiler batch string overflow\n");
		assert(0);
		return;
	}

	memory_copy((void*)(bat + *offset), src, size);
	*offset += size;
}
    
b8 graphics_shader_compile_file(const ShaderCompileDesc* desc, const char* src_path, Buffer* data)
{
	char filepath[FILE_PATH_SIZE];
	compute_random_path(filepath);

	char bat[BAT_SIZE];
	u32 offset = 0u;

	// .exe path
	append_bat(bat, &offset, "dxc.exe ");
	
	// API Specific
	switch (desc->api)
	{
	case GraphicsAPI_Vulkan:
	{
		append_bat(bat, &offset, "-spirv ");
		
		char shift_str[20];
		
		// Shift resources
		u32 shift = GraphicsLimit_ConstantBuffer;
		string_from_u32(shift_str, shift);
		append_bat(bat, &offset, "-fvk-t-shift ");
		append_bat(bat, &offset, shift_str);
		append_bat(bat, &offset, " all ");
		
		shift += GraphicsLimit_ShaderResource;
		string_from_u32(shift_str, shift);
		append_bat(bat, &offset, "-fvk-u-shift ");
		append_bat(bat, &offset, shift_str);
		append_bat(bat, &offset, " all ");
		
		shift += GraphicsLimit_UnorderedAccessView;
		string_from_u32(shift_str, shift);
		append_bat(bat, &offset, "-fvk-s-shift ");
		append_bat(bat, &offset, shift_str);
		append_bat(bat, &offset, " all ");
	}
	break;
	}

	// Target
	append_bat(bat, &offset, "-T ");
	switch (desc->shader_type)
	{
	case ShaderType_Vertex:
		append_bat(bat, &offset, "vs_");
		break;
	case ShaderType_Pixel:
		append_bat(bat, &offset, "ps_");
		break;
	case ShaderType_Geometry:
		append_bat(bat, &offset, "gs_");
		break;

	case ShaderType_Compute:
		append_bat(bat, &offset, "cs_");
		break;
	}

	{
		char version_str[20];
		
		string_from_u32(version_str, desc->major_version);
		append_bat(bat, &offset, version_str);
		
		append_bat(bat, &offset, "_");

		string_from_u32(version_str, desc->minor_version);
		append_bat(bat, &offset, version_str);
		
		append_bat(bat, &offset, " ");
	}

	// Entry point
	append_bat(bat, &offset, "-E ");
	append_bat(bat, &offset, desc->entry_point);
	append_bat(bat, &offset, " ");

	// Optimization level
	append_bat(bat, &offset, "-O3 ");

	// Include path
	{
		append_bat(bat, &offset, "-I / ");
	}

	// API Macro
	switch (desc->api)
	{
	case GraphicsAPI_Vulkan:
		append_bat(bat, &offset, "-D ");
		append_bat(bat, &offset, "SV_API_VULKAN ");
		break;
	}

	// Shader Macro

	switch (desc->shader_type)
	{
	case ShaderType_Vertex:
		append_bat(bat, &offset, "-D ");
		append_bat(bat, &offset, "SV_VERTEX_SHADER ");
		break;
	case ShaderType_Pixel:
		append_bat(bat, &offset, "-D ");
		append_bat(bat, &offset, "SV_PIXEL_SHADER ");
		break;
	case ShaderType_Geometry:
		append_bat(bat, &offset, "-D ");
		append_bat(bat, &offset, "SV_GEOMETRY_SHADER ");
		break;
	case ShaderType_Hull:
		append_bat(bat, &offset, "-D ");
		append_bat(bat, &offset, "SV_HULL_SHADER ");
		break;
	case ShaderType_Domain:
		append_bat(bat, &offset, "-D ");
		append_bat(bat, &offset, "SV_DOMAIN_SHADER ");
		break;
	case ShaderType_Compute:
		append_bat(bat, &offset, "-D ");
		append_bat(bat, &offset, "SV_COMPUTE_SHADER ");
		break;
	}

	// User Macro
	foreach(i, desc->macro_count) {

		ShaderMacro macro = desc->macros[i];

		if (string_size(macro.name) == 0u) continue;

		append_bat(bat, &offset, "-D ");
		append_bat(bat, &offset, macro.name);

		if (string_size(macro.value) != 0) {
			append_bat(bat, &offset, "=");
			append_bat(bat, &offset, macro.value);
		}

		append_bat(bat, &offset, " ");
	}

	// Input - Output
	append_bat(bat, &offset, src_path);
	append_bat(bat, &offset, " -Fo ");
	append_bat(bat, &offset, filepath);

	append_bat(bat, &offset, " 2> shader_log.txt ");

	bat[offset] = '\0';
	
	// Execute
	system(bat);

	// Read from file
	{
		u8* file_data;
		u32 file_size;
		if (!file_read_binary(filepath, &file_data, &file_size)) 
			return FALSE;

		buffer_set(data, file_data, file_size);
	}

	// Remove tem file
	if (!file_remove(filepath))
	{
		SV_LOG_ERROR("Can't remove the shader temporal file at '%s'\n", filepath);
	}

	return TRUE;
}

b8 graphics_shader_compile_fastbin_from_string(const char* name, ShaderType shader_type, Shader** shader, const char* src, b8 alwais_compile)
{
	Buffer data = buffer_init(1.f);
	u64 hash = hash_string(name);
	hash = hash_combine(hash, (u64)shader_type);

	ShaderDesc desc;
	desc.shader_type = shader_type;

	if (alwais_compile || !bin_read(hash, &data))
	{
		ShaderCompileDesc c;
		c.api = graphics_api();
		c.entry_point = "main";
		c.major_version = 6u;
		c.minor_version = 0u;
		c.shader_type = shader_type;
		c.macro_count = 0u;

		SV_CHECK(graphics_shader_compile_string(&c, src, (u32)string_size(src), &data));
		SV_CHECK(bin_write(hash, data.data, data.size));

		SV_LOG_INFO("Shader Compiled: '%s'\n", name);
	}

	desc.bin_data_size = data.size;
	desc.bin_data = data.data;
	return graphics_shader_create(shader, &desc);
}
	
b8 graphics_shader_compile_fastbin_from_file(const char* name, ShaderType shader_type, Shader** shader, const char* filepath, b8 alwais_compile)
{
	Buffer data = buffer_init(1.f);
	u64 hash = hash_string(name);
	hash = hash_combine(hash, (u64)shader_type);

	ShaderDesc desc;
	desc.shader_type = shader_type;

	if (alwais_compile || !bin_read(hash, &data))
	{
		u8* file_data = NULL;
		u32 file_size = 0;
		if (!file_read_text(filepath, &file_data, &file_size)) {
			SV_LOG_ERROR("Shader source not found: %s\n", filepath);
			return FALSE;
		}

		ShaderCompileDesc c;
		c.api = graphics_api();
		c.entry_point = "main";
		c.major_version = 6u;
		c.minor_version = 0u;
		c.shader_type = shader_type;
		c.macro_count = 0u;

		if (!graphics_shader_compile_string(&c, (const char*)file_data, file_size, &data)) {
			SV_LOG_ERROR("Can't compile the shader '%s'\n", filepath);
			return FALSE;
		}

		if (file_data)
			memory_free(file_data);

		SV_CHECK(bin_write(hash, data.data, data.size));

		SV_LOG_INFO("Shader Compiled: '%s'\n", name);
	}

	desc.bin_data_size = data.size;
	desc.bin_data = data.data;
	return graphics_shader_create(shader, &desc);
}

#endif
