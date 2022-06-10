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

////////////////////////////// DXC CALLS ///////////////////////////////

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

static b8 dxc_call(const ShaderCompileDesc* desc, const ShaderPreprocessorData* ppdata, const char* src_path, Buffer* data)
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
		append_bat(bat, &offset, "-DENABLE_SPIRV_CODEGEN=ON -spirv ");

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
	switch (ppdata->shader_type)
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

	switch (ppdata->shader_type)
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

////////////////////////////// PREPROCESSOR /////////////////////////

static void preprocess_shader(const char* shader, u32 src_size, char** out_, u32* size_, ShaderPreprocessorData* pp)
{
	u32 size = src_size;
	char* str = memory_allocate(src_size + 100);

	pp->shader_type = ShaderType_Vertex;
	
	char* it0 = str;
	const char* it1 = shader;

	while (*it1 != '\0') {

		const char* start_line = it1;

		it1 = line_jump_spaces(it1);

		if (*it1 == '#') {

			it1++;

			if (string_begins(it1, "shader ")) {

				start_line = NULL;

				it1 += string_size("shader ");
				it1 = line_jump_spaces(it1);
				
				if (string_begins(it1, "vertex")) {
					pp->shader_type = ShaderType_Vertex;
				}
				else if (string_begins(it1, "pixel")) {
					pp->shader_type = ShaderType_Pixel;
				}
				else if (string_begins(it1, "geometry")) {
					pp->shader_type = ShaderType_Geometry;
				}
				else if (string_begins(it1, "compute")) {
					pp->shader_type = ShaderType_Compute;
				}
			}
		}
		
		if (start_line != NULL) {

			it1 = start_line;

			while (*it1 != '\0' && *it1 != '\n') {

				// TODO: Safe
				*it0 = *it1;

				++it0;
				++it1;
			}

			if (*it1 != '\0') {
				*it0 = *it1;
				++it0;
				++it1;
			}
		}
		else it1 = line_next(it1);
	}

	*size_ = (u32)(it0 - str) + 1;
	*out_ = str;
}

/////////////////////////// COMPILE CALLS //////////////////////////////

b8 graphics_shader_compile_string(const ShaderCompileDesc* desc, const char* str, u32 size, Buffer* data, ShaderPreprocessorData* ppdata)
{
	ShaderPreprocessorData pp;
	char* shader;
	u32 shader_size;
	preprocess_shader(str, size, &shader, &shader_size, &pp);

	char filepath[FILE_PATH_SIZE];
	compute_random_path(filepath);
	
	b8 res = file_write_text(filepath, shader, shader_size, FALSE, TRUE);

	if (!res) {
		memory_free(shader);
		return FALSE;
	}

	res = dxc_call(desc, &pp, filepath, data);

	if (!res) {
		file_remove(filepath);
		memory_free(shader);
		return FALSE;
	}

	res = file_remove(filepath);

	if (ppdata) {
		*ppdata = pp;
	}

	memory_free(shader);
	return res;
}

//////////////////////////////////////// FAST BIN COMPILE CALLS /////////////////////////////////

b8 graphics_shader_compile_fastbin_from_string(const char* name, Shader** shader, const char* src, b8 alwais_compile)
{
	u8* data;
	u32 size;
	u64 hash = hash_string(name);

	ShaderPreprocessorData pp;
	ShaderDesc desc;

	// TODO

	if (TRUE)
	{
		ShaderCompileDesc c;
		c.api = graphics_api();
		c.entry_point = "main";
		c.major_version = 6u;
		c.minor_version = 0u;
		c.macro_count = 0u;

		Buffer buf = buffer_init(1.f);;

		SV_CHECK(graphics_shader_compile_string(&c, src, (u32)string_size(src), &buf, &pp));

		data = buf.data;
		size = buf.size;

		desc.shader_type = pp.shader_type;

		SV_LOG_INFO("Shader Compiled: '%s'\n", name);
	}

	desc.bin_data_size = size;
	desc.bin_data = data;
	return graphics_shader_create(shader, &desc);
}
	
b8 graphics_shader_compile_fastbin_from_file(Shader** shader, const char* filepath, b8 recompile)
{
	u64 hash = hash_string(filepath);
	ShaderPreprocessorData pp;
	ShaderDesc desc;

	u8* bin = NULL;
	u32 bin_size = 0;

	// Try to read from bin data
	if (!recompile) {

		u8* data;
		u32 size;

		if (bin_read(hash, &data, &size))
		{
			Deserializer s;
			deserializer_begin_buffer(&s, data, size);

			deserialize_u32(&s, (u32*)(&desc.shader_type));
			deserialize_u32(&s, &bin_size);

			bin = memory_allocate(bin_size);

			deserializer_read(&s, bin, bin_size);

			deserializer_end_buffer(&s);

			memory_free(data);
		}
	}

	if (bin == NULL)
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
		c.macro_count = 0u;

		// TODO: Use simple buffer
		Buffer data;
		data = buffer_init(1.f);

		if (!graphics_shader_compile_string(&c, (const char*)file_data, file_size, &data, &pp)) {
			SV_LOG_ERROR("Can't compile the shader '%s'\n", filepath);
			return FALSE;
		}

		desc.shader_type = pp.shader_type;

		if (file_data)
			memory_free(file_data);

		bin = data.data;
		bin_size = data.size;

		// Save bin data
		{
			u8 buff[10000];

			Serializer s;
			serializer_begin_buffer(&s, buff, 10000);

			serialize_u32(&s, (u32)desc.shader_type);
			serialize_u32(&s, data.size);

			serializer_write(&s, data.data, data.size);

			SV_CHECK(bin_write(hash, s.data, s.cursor));

			serializer_end_buffer(&s);
		}

		SV_LOG_INFO("Shader Compiled: '%s'\n", filepath);
	}

	desc.bin_data_size = bin_size;
	desc.bin_data = bin;
	b8 res = graphics_shader_create(shader, &desc);

	memory_free(bin);

	return res;
}

#endif
