#include "Hosebase/shader_compiler.h"

#include "Hosebase/serialize.h"

static void compute_bin_shader_filepath(char *dst, const char *filepath, GraphicsAPI api)
{
	string_copy(dst, "shader_bin/", FILE_PATH_SIZE);

	char shader_name[FILE_PATH_SIZE];
	string_copy(shader_name, filepath, FILE_PATH_SIZE);
	string_replace_char(shader_name, '/', '_');
	string_replace_char(shader_name, '.', '_');

	if (api == GraphicsAPI_Vulkan)
		string_append(shader_name, ".spirv", FILE_PATH_SIZE);
	else // TODO
		string_append(shader_name, ".spirv", FILE_PATH_SIZE);

	string_append(dst, shader_name, FILE_PATH_SIZE);
}

#if SV_SHADER_COMPILER

#include "Hosebase/external/sprv/spirv_cross_c.h"
#include "Hosebase/src/graphics/graphics_internal.h"

typedef struct
{
	ShaderType shader_type;
} ShaderPreprocessorData;

b8 shader_compiler_initialize()
{
	return TRUE;
}

b8 shader_compiler_close()
{
	return TRUE;
}

inline void compute_random_path(char *str)
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

inline void append_bat(char *bat, u32 *offset, const char *src)
{
	u32 size = string_size(src);
	if (size + *offset > BAT_SIZE)
	{
		SV_LOG_ERROR("Shader compiler batch string overflow\n");
		assert(0);
		return;
	}

	memory_copy((void *)(bat + *offset), src, size);
	*offset += size;
}

static b8 dxc_call(const ShaderCompileDesc *desc, const ShaderPreprocessorData *ppdata, const char *src_path, u8 **out_data, u32 *out_size)
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
	foreach (i, desc->macro_count)
	{

		ShaderMacro macro = desc->macros[i];

		if (string_size(macro.name) == 0u)
			continue;

		append_bat(bat, &offset, "-D ");
		append_bat(bat, &offset, macro.name);

		if (string_size(macro.value) != 0)
		{
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
		if (!file_read_binary(FilepathType_Asset, filepath, out_data, out_size))
			return FALSE;
	}

	// Remove tem file
	if (!file_remove(FilepathType_Asset, filepath))
	{
		SV_LOG_ERROR("Can't remove the shader temporal file at '%s'\n", filepath);
	}

	return TRUE;
}

////////////////////////////// PREPROCESSOR /////////////////////////

static void preprocess_shader(const char *shader, u32 src_size, char **out_, u32 *size_, ShaderPreprocessorData *pp)
{
	u32 size = src_size;
	char *str = memory_allocate(src_size + 100);

	pp->shader_type = ShaderType_Vertex;

	char *it0 = str;
	const char *it1 = shader;

	while (*it1 != '\0')
	{

		const char *start_line = it1;

		it1 = line_jump_spaces(it1);

		if (*it1 == '#')
		{

			it1++;

			if (string_begins(it1, "shader "))
			{

				start_line = NULL;

				it1 += string_size("shader ");
				it1 = line_jump_spaces(it1);

				if (string_begins(it1, "vertex"))
				{
					pp->shader_type = ShaderType_Vertex;
				}
				else if (string_begins(it1, "pixel"))
				{
					pp->shader_type = ShaderType_Pixel;
				}
				else if (string_begins(it1, "geometry"))
				{
					pp->shader_type = ShaderType_Geometry;
				}
				else if (string_begins(it1, "compute"))
				{
					pp->shader_type = ShaderType_Compute;
				}
			}
		}

		if (start_line != NULL)
		{

			it1 = start_line;

			while (*it1 != '\0' && *it1 != '\n')
			{

				// TODO: Safe
				*it0 = *it1;

				++it0;
				++it1;
			}

			if (*it1 != '\0')
			{
				*it0 = *it1;
				++it0;
				++it1;
			}
		}
		else
			it1 = line_next(it1);
	}

	*size_ = (u32)(it0 - str) + 1;
	*out_ = str;
}

/////////////////////////// COMPILE CALLS //////////////////////////////

b8 shader_compile(ShaderDesc *result, const ShaderCompileDesc *desc, const char *filepath, b8 save)
{
	memory_zero(result, sizeof(ShaderDesc));

	char temp_filepath[FILE_PATH_SIZE];
	compute_random_path(temp_filepath);

	char *shader_source = NULL;
	u32 shader_source_size = 0;

	u8 *shader_binary = NULL;
	u32 shader_binary_size = 0;

	char *shader_processed = NULL;
	u32 shader_processed_size = 0;

	if (!file_read_text(FilepathType_Asset, filepath, &shader_source, &shader_source_size))
	{
		SV_LOG_ERROR("Can't find the shader '%s'\n", filepath);
		return FALSE;
	}

	ShaderPreprocessorData pp;
	preprocess_shader(shader_source, shader_source_size, &shader_processed, &shader_processed_size, &pp);

	b8 res = file_write_text(FilepathType_Asset, temp_filepath, shader_processed, shader_processed_size, FALSE, TRUE);

	if (!res)
	{
		goto end;
	}

	res = dxc_call(desc, &pp, temp_filepath, &shader_binary, &shader_binary_size);

	if (!res)
	{
		goto end;
	}

	// Fill result
	{
		result->type = pp.shader_type;
		result->bin_data = shader_binary;
		result->bin_data_size = shader_binary_size;
	}

	// Process spirv resources
	if (desc->api == GraphicsAPI_Vulkan)
	{
		const SpvId *spirv = (SpvId *)shader_binary;
		size_t word_count = shader_binary_size / sizeof(SpvId);

		// Create context.
		spvc_context context = NULL;
		spvc_context_create(&context);

		spvc_parsed_ir ir = NULL;
		spvc_context_parse_spirv(context, spirv, word_count, &ir);

		spvc_compiler compiler = NULL;
		spvc_context_create_compiler(context, SPVC_BACKEND_NONE, ir, SPVC_CAPTURE_MODE_COPY, &compiler);

		spvc_resources resources = NULL;
		spvc_compiler_create_shader_resources(compiler, &resources);

		// Semantic Names
		if (result->type == ShaderType_Vertex)
		{
			const spvc_reflected_resource *inputs = NULL;
			size_t count;
			spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_STAGE_INPUT, &inputs, &count);
			result->semantic_name_count = count;

			assert(result->semantic_name_count <= sizeof(result->semantic_names));

			foreach (i, result->semantic_name_count)
			{
				spvc_reflected_resource input = inputs[i];

				const char *name = input.name + 7;

				u32 location = spvc_compiler_get_decoration(compiler, input.id, SpvDecorationLocation);
				result->semantic_names[i].location = location;
				string_copy(result->semantic_names[i].name, name, NAME_SIZE);
			}
		}

		// Constant Buffers
		{
			const spvc_reflected_resource *uniforms = NULL;
			size_t count;
			spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_UNIFORM_BUFFER, &uniforms, &count);

			assert(count <= GraphicsLimit_ConstantBuffer);

			foreach (i, count)
			{
				spvc_reflected_resource uniform = uniforms[i];

				if (string_equals(uniform.name, "type__Globals"))
				{
					continue;
				}

				u32 binding = spvc_compiler_get_decoration(compiler, uniform.id, SpvDecorationBinding);

				u32 index = result->resource_count++;
				result->resources[index].type = ShaderResourceType_ConstantBuffer;
				result->resources[index].binding = binding;
				string_copy(result->resources[index].name, uniform.name, NAME_SIZE);
			}
		}

		// Shader Resources
		{
			const spvc_reflected_resource *images = NULL;
			size_t count;
			spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_SEPARATE_IMAGE, &images, &count);
			u32 image_count = count;

			const spvc_reflected_resource *storages = NULL;
			spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_STORAGE_BUFFER, &storages, &count);
			u32 storage_count = count;

			assert(image_count + storage_count <= GraphicsLimit_ConstantBuffer);

			foreach (i, image_count)
			{
				spvc_reflected_resource image = images[i];

				u32 binding = spvc_compiler_get_decoration(compiler, image.id, SpvDecorationBinding);

				u32 index = result->resource_count++;
				result->resources[index].binding = binding;
				string_copy(result->resources[index].name, image.name, NAME_SIZE);

				spvc_type type = spvc_compiler_get_type_handle(compiler, image.type_id);
				SpvDim dimension = spvc_type_get_image_dimension(type);

				if (dimension == SpvDim2D || dimension == SpvDimCube)
				{
					result->resources[index].type = ShaderResourceType_Image;
				}
				else
				{
					result->resources[index].type = ShaderResourceType_TexelBuffer;
				}
			}

			foreach (i, storage_count)
			{
				spvc_reflected_resource storage = storages[i];

				u32 binding = spvc_compiler_get_decoration(compiler, storage.id, SpvDecorationBinding);

				u32 index = result->resource_count++;
				result->resources[index].type = ShaderResourceType_StorageBuffer;
				result->resources[index].binding = binding;
				string_copy(result->resources[index].name, storage.name, NAME_SIZE);
			}
		}

		// Unordered Access View
		{
			const spvc_reflected_resource *storages = NULL;
			size_t count;
			spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_STORAGE_IMAGE, &storages, &count);
			u32 storage_count = count;

			assert(storage_count <= GraphicsLimit_UnorderedAccessView);

			foreach (i, storage_count)
			{
				spvc_reflected_resource storage = storages[i];

				u32 binding = spvc_compiler_get_decoration(compiler, storage.id, SpvDecorationBinding);

				u32 index = result->resource_count++;
				result->resources[index].binding = binding;
				string_copy(result->resources[index].name, storage.name, NAME_SIZE);

				spvc_type type = spvc_compiler_get_type_handle(compiler, storage.type_id);
				spvc_basetype basetype = spvc_type_get_basetype(type);

				if (basetype == SPVC_BASETYPE_IMAGE || basetype == SPVC_BASETYPE_SAMPLED_IMAGE)
				{
					result->resources[index].type = ShaderResourceType_UAImage;
				}
				else
				{
					result->resources[index].type = ShaderResourceType_UATexelBuffer;
				}
			}
		}

		// Samplers
		{
			const spvc_reflected_resource *samplers = NULL;
			size_t count;
			spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_SEPARATE_SAMPLERS, &samplers, &count);
			u32 sampler_count = count;

			assert(sampler_count <= GraphicsLimit_Sampler);

			foreach (i, sampler_count)
			{
				spvc_reflected_resource sampler = samplers[i];

				u32 binding = spvc_compiler_get_decoration(compiler, sampler.id, SpvDecorationBinding);

				u32 index = result->resource_count++;
				result->resources[index].type = ShaderResourceType_Sampler;
				result->resources[index].binding = binding;
				string_copy(result->resources[index].name, sampler.name, NAME_SIZE);
			}
		}

		spvc_context_destroy(context);
	}

	if (save)
	{
		Serializer s;
		serializer_begin_file(&s, shader_binary_size + 2000);

		char temp_filepath[FILE_PATH_SIZE];
		compute_bin_shader_filepath(temp_filepath, filepath, desc->api);

		{
			serialize_u32(&s, 0); // VERSION

			// Misc
			{
				serialize_u32(&s, (u32)result->type);
			}

			// Semantic names
			{
				serialize_u32(&s, result->semantic_name_count);

				foreach (i, result->semantic_name_count)
				{
					serialize_string(&s, result->semantic_names[i].name);
					serialize_u32(&s, result->semantic_names[i].location);
				}
			}

			// Resources
			{
				serialize_u32(&s, result->resource_count);

				foreach (i, result->resource_count)
				{
					serialize_string(&s, result->resources[i].name);
					serialize_u32(&s, (u32)result->resources[i].type);
					serialize_u32(&s, result->resources[i].binding);
				}
			}

			// Finally, SPIRV code
			serialize_u32(&s, shader_binary_size);
			serializer_write(&s, shader_binary, shader_binary_size);
		}

		if (!serializer_end_file(&s, FilepathType_Asset, temp_filepath))
		{
			SV_LOG_ERROR("Can't save shader compilation '%s'\n", filepath);
		}
	}

	res = TRUE;

end:
	memory_free(shader_source);
	if (!res)
		memory_free(shader_binary);
	memory_free(shader_processed);
	file_remove(FilepathType_Asset, temp_filepath);

	if (res)
		SV_LOG_INFO("Shader '%s' compiled\n", filepath);
	else
		SV_LOG_ERROR("Can't compile shader '%s'\n", filepath);

	return res;
}

#endif

b8 shader_read_binary(ShaderDesc *result, GraphicsAPI api, const char *filepath)
{
	memory_zero(result, sizeof(ShaderDesc));

	char temp_filepath[FILE_PATH_SIZE];
	compute_bin_shader_filepath(temp_filepath, filepath, api);

	Deserializer s;
	if (deserializer_begin_file(&s, FilepathType_Asset, temp_filepath))
	{
		u32 version;
		deserialize_u32(&s, &version);

		// Misc
		{
			deserialize_u32(&s, (u32 *)&result->type);
		}

		// Semantic names
		{
			deserialize_u32(&s, &result->semantic_name_count);

			foreach (i, result->semantic_name_count)
			{
				deserialize_string(&s, result->semantic_names[i].name, SV_ARRAY_SIZE(result->semantic_names[i].name));
				deserialize_u32(&s, &result->semantic_names[i].location);
			}
		}

		// Resources
		{
			deserialize_u32(&s, &result->resource_count);

			foreach (i, result->resource_count)
			{
				deserialize_string(&s, result->resources[i].name, SV_ARRAY_SIZE(result->resources[i].name));
				deserialize_u32(&s, (u32 *)&result->resources[i].type);
				deserialize_u32(&s, &result->resources[i].binding);
			}
		}

		// Finally, SPIRV code
		deserialize_u32(&s, &result->bin_data_size);
		result->bin_data = memory_allocate(result->bin_data_size);
		deserializer_read(&s, result->bin_data, result->bin_data_size);

		deserializer_end_file(&s);
	}
	else
		return FALSE;

	return TRUE;
}