#include "Hosebase/serialize.h"

#include "Hosebase/memory_manager.h"

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
    
inline void bin_filepath(char* buf, u64 hash)
{
	sprintf(buf, "bin/%llu.bin", hash);
}

b8 bin_read(u64 hash, Buffer* data)
{
	char filepath[BIN_PATH_SIZE];
	bin_filepath(filepath, hash);

	u8* d;
	u32 size;
	if (!file_read_binary(filepath, &d, &size)) return FALSE;

	buffer_set(data, d, size);
	return TRUE;
}
    
/*b8 bin_read(u64 hash, Deserializer& deserializer, b8 system)
  {
  char filepath[BIN_PATH_SIZE];
  bin_filepath(filepath, hash, system);
  return deserialize_begin(deserializer, filepath);
  }*/

b8 bin_write(u64 hash, const void* data, u32 size)
{
	char filepath[BIN_PATH_SIZE];
	bin_filepath(filepath, hash);
	return file_write_binary(filepath, (u8*)data, size, FALSE, TRUE);
}
    
/*b8 bin_write(u64 hash, Serializer& serializer, b8 system)
  {
  char filepath[BIN_PATH_SIZE];
  bin_filepath(filepath, hash, system);
  return serialize_end(serializer, filepath);
  }*/

///////////////////////////////////////////// XML LOADER ////////////////////////////////////////////

inline b8 xml_string_equals(const char* str0, const char* str1)
{
	while (*str0 != '\0' && *str1 != '\0') {

		if (*str0 == '>' || *str0 == '/' || *str0 == ' ') {
			str0 = " ";
		}
		if (*str1 == '>' || *str1 == '/' || *str1 == ' ') {
			str1 = " ";
		}

		if (*str0 != *str1) {
			return FALSE;
		}

		++str0;
		++str1;
	}

	return *str0 == *str1;
}

inline const char* xml_exit_tag(const char* c)
{
	while (*c != '\0' && *c != '>') {

		if (*c == '/' && *(c + 1) == '>') {
			++c;
			break;
		}

		++c;
	}

	if (*c == '\0') return c;

	return c + 1;
}

inline const char* xml_find_end(const char* begin)
{
	const char* c = begin + 1;
	const char* name_begin = c;
	i32 level = 0;

	// Exit from tag
	{
		while (*c != '\0' && *c != '>') {

			if (*c == '/' && *(c + 1) == '>') {
				break;
			}

			++c;
		}

		if (*c == '\0') return NULL;
		if (*c == '/') {
			return c + 2;
		}
		else {
			++c;
		}
	}

	// Find close tag
	{
		while (1) {
			while (*c != '\0' && *c != '<') {

				if (*c == '/' && *(c + 1) == '>') {
					level--;
					++c;
				}

				++c;
			}

			if (*c == '\0')
				return NULL;

			++c;

			if (*c == '/') {

				++c;

				if (level == 0 && xml_string_equals(name_begin, c)) {

					while (*c != '\0' && *c != '>') {
						++c;
					}

					if (*c == '>') {

						return c + 1;
					}
					else return NULL;
				}
				else {
					level--;
				}
			}
			else level++;
		}
	}

	return begin;
}

inline b8 xml_string_equals_to_normal(const char* xml_str, const char* normal)
{
	while (*xml_str != '\0' && *normal != '\0') {

		if (*xml_str == '/' || *xml_str == '>' || *xml_str == ' ' || *xml_str == '=') {
			xml_str = "";
		}

		if (*xml_str != *normal) {
			return FALSE;
		}

		++xml_str;
		++normal;
	}

	if (*xml_str == '/' || *xml_str == '>' || *xml_str == ' ' || *xml_str == '=') {
		xml_str = "";
	}

	return *xml_str == *normal;
}

XMLElement xml_begin(const char* data, u32 size)
{
	XMLElement e;
	e.data = data;
	e.size = size;
	e.corrupted = FALSE;
	e.level = 0;

	const char* c = line_jump_spaces(e.data);

	while (!e.corrupted) {

		if (*c == '<') {

			e.begin = c;

			++c;

			if (*c == '?') {

				while (*c != '>' && *c != '\0')
					c++;

				if (*c == '>') {
					++c;
				}
				else {
					e.corrupted = TRUE;
				}
			}
			else {

				const char* end = xml_find_end(e.begin);

				if (end) {
					e.end = end;
					break;
				}
				else {
					e.corrupted;
				}
			}
		}
		else if (*c == '\n' || *c == ' ' || *c == '\r' || *c == '\t') {
			++c;
		}
		else {
			e.corrupted = TRUE;
		}
	}

	return e;
}

u32 xml_name(const XMLElement* e, char* buffer, u32 buffer_size)
{
	const char* begin = e->begin + 1;
	const char* end = begin;

	while (*end != '\0' && *end != '/' && *end != ' ' && *end != '>') {
		++end;
	}

	u32 size = end - begin;

	if (buffer_size == 0)
		return size;

	u32 copy = SV_MIN(size, buffer_size - 1);

	memory_copy(buffer, begin, copy);
	buffer[copy] = '\0';

	return size - copy;
}

b8 xml_enter_child(XMLElement* e, const char* name)
{
	const char* c = xml_exit_tag(e->begin);

	while (1) {

		while (*c != '\0' && *c != '<' && *c != '>') {
			++c;
		}

		if (*c == '\0') {
			e->corrupted = TRUE;
			return FALSE;
		}
		else if (*c == '>') {
			return FALSE;
		}
		else if (*(c + 1) == '/') {
			return FALSE;
		}

		++c;

		const char* begin = c - 1;
		const char* end = xml_find_end(begin);

		if (end == NULL) {
			e->corrupted = TRUE;
			return FALSE;
		}

		if (xml_string_equals_to_normal(c, name)) {

			e->begin = begin;
			e->end = end;
			e->level++;
			break;
		}
		else {

			c = end;
		}
	}

	return TRUE;

corrupted:
	e->corrupted = TRUE;
	return FALSE;
}

b8 xml_next(XMLElement* e)
{
	if (e->level == 0)
		return FALSE;

	const char* c = e->end;

	while (1) {

		while (*c != '\0' && *c != '<' && *c != '/') {
			++c;
		}

		if (*c == '\0') {
			e->corrupted = TRUE;
			return FALSE;
		}
		else if (*c == '/') {
			return FALSE;
		}
		else if (*(c + 1) == '/')
			return FALSE;

		++c;

		const char* begin = c - 1;
		const char* end = xml_find_end(begin);

		if (end == NULL) {
			e->corrupted = TRUE;
			return FALSE;
		}

		if (xml_string_equals(c, e->begin + 1)) {

			e->begin = begin;
			e->end = end;
			break;
		}
		else {

			c = end;
		}
	}

	return TRUE;
}

b8 xml_element_content(XMLElement* e, const char** pbegin, const char** pend)
{
	// Begin
	{
		const char* c = e->begin + 1;

		while (*c != '\0' && *c != '>' && *c != '/') {
			++c;
		}

		if (*c == '/')
			return FALSE;
		else if (*c == '\0') {
			e->corrupted = TRUE;
			return FALSE;
		}

		++c;
		*pbegin = c;
	}

	// End
	{
		const char* c = e->end - 1;
		while ((e->data + 4) < c && *c != '/' && *c != '<') {
			--c;
		}

		if (*c == '/' && *(c - 1) == '<') {
			*pend = c - 1;
			return TRUE;
		}
		else {
			e->corrupted = TRUE;
			return FALSE;
		}
	}
}

b8 xml_get_attribute(XMLElement* e, char* buffer, u32 buffer_size, const char* att_name)
{
	if (buffer_size == 0 || e->corrupted)
		return FALSE;

	const char* c = e->begin + 1;
	while (*c != '\0' && *c != '>') {

		if (*c == ' ' || *c == '\n' || *c == '\t') {

			while (*c == ' ' || *c == '\n' || *c == '\t') {
				++c;
			}

			if (*c == '\0' || *c == '>' || *c == '/') {
				return FALSE;
			}

			const char* start_name = c;

			while (*c != '\0' && *c != '>' && *c != '=')
				++c;

			if (*c == '\0' || *c == '>' || *c == '/') {
				return FALSE;
			}

			if (*c == '=' && xml_string_equals_to_normal(start_name, att_name)) {

				++c;
				while (*c == ' ')
					++c;

				const char* begin = c + 1;

				if (*c == '"') {

					++c;

					while (*c != '\0' && *c != '"')
						++c;

					if (*c == '\0') {
						e->corrupted = TRUE;
						return FALSE;
					}

					const char* end = c;

					u32 size = end - begin;
					u32 copy = SV_MIN(size, buffer_size - 1);
					memory_copy(buffer, begin, copy);
					buffer[copy] = '\0';
					return TRUE;
				}
			}
		}

		++c;
	}

	return FALSE;
}

b8 xml_get_attribute_u32(XMLElement* e, u32* n, const char* att_name)
{
	char str[20];
	if (xml_get_attribute(e, str, 20, att_name)) {

		return string_to_u32(n, str);
	}
	return FALSE;
}

///////////////////////////////////////////// OBJ FORMAT /////////////////////////////////////////////

inline void set_default_material(MaterialInfo* mat)
{
	// Defaults
	mat->ambient_color = color_rgba_f32(0.2f, 0.2f, 0.2f, 1.f);
	mat->diffuse_color = color_rgba_f32(0.8f, 0.8f, 0.8f, 1.f);
	mat->specular_color = color_rgba_f32(1.f, 1.f, 1.f, 1.f);
	mat->emissive_color = color_rgba_f32(0.f, 0.f, 0.f, 1.f);
	mat->shininess = 1.f;
}

inline void read_mtl(const char* filepath, ModelInfo* model_info)
{
	u8* file_data;
	u32 file_size;
	if (file_read_text(filepath, &file_data, &file_size)) {

		MaterialInfo* mat = model_info->materials + model_info->material_count++;
		set_default_material(mat);
		b8 use_default = TRUE;

		const char* it = file_data;
		u32 line_count = 0;
		b8 corrupted = FALSE;
		
		while (*it != '\0' && !corrupted) {
			
			it = line_jump_spaces(it);
			
			switch (*it) {
				
			case 'm':
			{
				++it;
				
				if (it[0] == 'a' && it[1] == 'p' && it[2] == '_') {
					
					it += 3u;
					
					char* str = NULL;

					if (it[0] == 'K' && it[1] == 'd') { // Diffuse map
						str = mat->diffuse_map_path;
					}
					else if (it[0] == 'K' && it[1] == 's') { // Specular map
						str = mat->specular_map_path;
					}
					else if (it[0] == 'K' && it[1] == 'a') { // TODO: Ambient map
						str = NULL;
					}
					else if (it[0] == 'K' && it[1] == 'n') { // Normal map
						str = mat->normal_map_path;
					}

					if (str) {

						it += 2;
						
						it = line_jump_spaces(it);
						
						char delimiters[] = {
							' ',
							'\n',
							'\r'
						};
		    
						u32 name_size = string_split(it, delimiters, 3u);
						if (name_size == 0u) {
							
							SV_LOG_ERROR("Can't read the mtl texture path in line %u", line_count + 1);
							corrupted = TRUE;
						}
						else {

							memory_copy(str, it, name_size);
							str[name_size] = '\0';
							path_clear(str);
						}
					}
				}
			}
			break;

			case 'i':
			{
				++it;

				if (it[0] == 'l' && it[1] == 'l' && it[2] == 'u' && it[3] == 'm') {

					it += 4u;

					it = line_jump_spaces(it);

					const char* delimiters = " \r\n";

					i32 value;
					b8 res;

					it = line_read_i32(it, &value, delimiters, (u32)string_size(delimiters), &res);
					
					if (res) {

						if (value < 0 || value > 10)
							res = FALSE;
						else {

							switch (value) {

							case 0: // Color on and Ambient off
								mat->transparent = FALSE;
								mat->culling = RasterizerCullMode_Back;
								break;
				    
							case 1: // Color on and Ambient on
								mat->transparent = FALSE;
								mat->culling = RasterizerCullMode_Back;
								break;
				    
							case 2: // Highlight on
								mat->transparent = FALSE;
								mat->culling = RasterizerCullMode_Back;
								break;
				    
							case 3: // Reflection on and Ray trace on
								mat->transparent = FALSE;
								mat->culling = RasterizerCullMode_Back;
								break;
				    
							case 4: // Transparency: Glass on. Reflection: Ray trace on
								mat->transparent = FALSE;
								mat->culling = RasterizerCullMode_None;
								break;
				    
							case 5:	// Reflection: Fresnel on and Ray trace on
								mat->transparent = FALSE;
								mat->culling = RasterizerCullMode_Back;
								break;
				    
							case 6: // Transparency: Refraction on. Reflection: Fresnel off and Ray trace on
								mat->transparent = TRUE;
								mat->culling = RasterizerCullMode_None;
								break;
				    
							case 7: // Transparency: Refraction on. Reflection: Fresnel on and Ray trace on
								mat->transparent = TRUE;
								mat->culling = RasterizerCullMode_None;
								break;
				    
							case 8: // Reflection on and Ray trace off
								mat->transparent = FALSE;
								mat->culling = RasterizerCullMode_Back;
								break;
				    
							case 9:	// Transparency: Glass on. Reflection: Ray trace off
								mat->transparent = TRUE;
								mat->culling = RasterizerCullMode_None;
								break;
				    
							case 10:// Casts shadows onto invisible surfaces
								mat->transparent = FALSE;
								mat->culling = RasterizerCullMode_Back;
								break;
				    
							}
						}
					}
					else res = FALSE;
			
					if (!res) {

						SV_LOG_ERROR("Illum value not available in mtl '%s', line %u", filepath, line_count + 1);
					}
				}
			}
			break;

			case 'n':
			{
				++it;
				if (it[0] == 'e' && it[1] == 'w' && it[2] == 'm' && it[3] == 't' && it[4] == 'l') {

					it += 5;
					it = line_jump_spaces(it);
						
					char delimiters[] = {
						' ',
						'\n',
						'\r'
					};
						
					u32 name_size = string_split(it, delimiters, 3u);
					if (name_size == 0u) {
			
						SV_LOG_ERROR("Can't read the mtl name in line %u", line_count + 1);
						corrupted = TRUE;
					}
					else {

						if (use_default) {

							use_default = FALSE;
						}
						else {
							if (model_info->material_count == MODEL_INFO_MAX_MATERIALS) {
								SV_LOG_ERROR("The file '%s' exceeds the material limit\n", filepath);
								return;
							}

							mat = model_info->materials + model_info->material_count;
						}

						string_set(mat->name, it, name_size, NAME_SIZE);
						set_default_material(mat);
					}
				}
			}
			break;

			case 'K':
			{
				++it;
				switch (*it++) {

				case 'a':
				{
					v3 color;

					b8 res;
					it = line_read_v3(it, &color, &res);
					
					if (res) {

						mat->ambient_color = color_rgb_f32(color.x, color.y, color.z);
					}
					else {
						corrupted = TRUE;
						SV_LOG_ERROR("Can't read the mtl ambient color in line %u", line_count + 1);
					}
				}
				break;
		    
				case 'd':
				{
					v3 color;
					b8 res;

					it = line_read_v3(it, &color, &res);
					
					if (res) {

						mat->diffuse_color = color_rgb_f32(color.x, color.y, color.z);
					}
					else {
						corrupted = TRUE;
						SV_LOG_ERROR("Can't read the mtl diffuse color in line %u", line_count + 1);
					}   
				}
				break;
		    
				case 's':
				{
					v3 color;
					b8 res;

					it = line_read_v3(it, &color, &res);
					
					if (res) {

						mat->specular_color = color_rgb(color.x, color.y, color.z);
					}
					else {
						corrupted = TRUE;
						SV_LOG_ERROR("Can't read the mtl specular color in line %u", line_count + 1);
					}
				}
				break;
		    
				case 'e':
				{
					v3 color;

					b8 res;
					it = line_read_v3(it, &color, &res);
					
					if (res) {
						
						mat->emissive_color = color_rgb(color.x, color.y, color.z);
					}
					else {
						corrupted = TRUE;
						SV_LOG_ERROR("Can't read the mtl emissive color in line %u", line_count + 1);
					}
				}
				break;

				default:
					corrupted = TRUE;
			
				}
			}
			break;

			case 'N':
			{
				++it;

				switch (*it++) {
			
				case 's': // Shininess
				{
					it = line_read_f32(it, &mat->shininess, NULL);
				}
				break;

				case 'i': // TODO: index of refraction
				{
			
				}
				break;
			
				}		    
			}
			break;
			}	

			it = line_next(it);
		}

		/*for (const MaterialInfo& m : model_info.materials) {

		  SV_LOG("Material '%s'", m.name.c_str());
		  SV_LOG("Ambient %u / %u / %u", m.ambient_color.r, m.ambient_color.g, m.ambient_color.b);
		  SV_LOG("Diffuse %u / %u / %u", m.diffuse_color.r, m.diffuse_color.g, m.diffuse_color.b);
		  SV_LOG("Specular %u / %u / %u", m.specular_color.r, m.specular_color.g, m.specular_color.b);
		  SV_LOG("Emissive %u / %u / %u", m.emissive_color.r, m.emissive_color.g, m.emissive_color.b);
		  if (m.diffuse_map_path.c_str()) SV_LOG("Diffuse map %s", m.diffuse_map_path.c_str());
		  if (m.normal_map_path.c_str()) SV_LOG("Normal map %s", m.normal_map_path.c_str());
		  }*/
	}
	else SV_LOG_ERROR("Can't load the mtl file at '%s'", filepath);
}

inline u32 parse_objindex_to_absolute(i32 i, i32 max)
{
	if (i > 0) {
		--i;
	}
	else if (i < 0) {
		i = max + i;
	}
	else i = 0;

	return (u32)i;
}

#define OBJ_POLYGON_COUNT 4

typedef struct {
	i32 position_indices[3u];
	i32 normal_indices[3u];
	i32 texcoord_indices[3u];
	b8 smooth;
} ObjTriangle;

typedef struct {
	char name[NAME_SIZE];
	DynamicArray(ObjTriangle) triangles;
	u32 material_index;
} ObjMesh;

inline ObjMesh* new_obj_mesh(DynamicArray* array)
{
	ObjMesh* mesh = array_add(array);
	mesh->triangles = array_init(ObjTriangle, 2.f);
	mesh->material_index = u32_max;
	return mesh;
}

static b8 model_load_obj(ModelInfo* model_info, const char* filepath, const char* it)
{
	DynamicArray(v3) positions = array_init(v3, 1.7f);
	DynamicArray(v3) normals = array_init(v3, 1.7f);
	DynamicArray(v2) texcoords = array_init(v2, 1.7f);
	DynamicArray(ObjMesh) meshes = array_init(ObjMesh, 1.7f);
	    
	ObjMesh* mesh = new_obj_mesh(&meshes);
	mesh->triangles = array_init(ObjTriangle, 2.f);
		
	mesh->material_index = u32_max;
	b8 using_default = TRUE;
	b8 smooth = TRUE;
	
	u32 line_count = 0;
	    
	b8 corrupted = FALSE;
		
	while (*it != '\0' && !corrupted) {

		it = line_jump_spaces(it);
			
		switch (*it) {
				
		case '#': // Comment
			break;
				
		case 'u': // use mtl
		{
			++it;
			if (string_begins(it, "semtl")) {
					
				it += 5u;
				it = line_jump_spaces(it);
					
				char delimiters[] = {
					' ',
					'\n',
					'\r'
				};
					
				u32 name_size = string_split(it, delimiters, 3u);
				if (name_size == 0u) {
						
					SV_LOG_ERROR("Can't read the mtl name in line %u", line_count + 1);
					corrupted = TRUE;
				}
				else {
						
					char texpath[FILE_PATH_SIZE];
					string_set(texpath, it, name_size, FILE_PATH_SIZE);
						
					u32 index = u32_max;
						
					foreach(i, model_info->material_count) {
							
						const MaterialInfo* m = model_info->materials + i;
							
						if (string_equals(m->name, texpath)) {
							index = i;
							break;
						}
					}
					
					if (index == u32_max) {
							
						SV_LOG_ERROR("Material %s not found, line %u\n", texpath, line_count + 1);
					}
					else {
							
						mesh->material_index = index;
					}
				}
			}
		}
		break;
		    
		case 'm': // .mtl path
		{
			++it;
			if (string_begins(it, "tllib")) {

				it += 5u;
				it = line_jump_spaces(it);
					
				char delimiters[] = {
					' ',
					'\n',
					'\r'
				};
					
				u32 name_size = string_split(it, delimiters, 3u);
				if (name_size == 0u) {
						
					SV_LOG_ERROR("Can't read the mtl path in line %u", line_count + 1);
					corrupted = TRUE;
				}
				else {
						
					char mtlname[FILE_PATH_SIZE] = "";
					string_set(mtlname, it, name_size, FILE_PATH_SIZE);
						
					char mtlpath[FILE_PATH_SIZE];
					string_copy(mtlpath, filepath, FILE_PATH_SIZE);
						
					u32 s = string_size(mtlpath) - 1u;
						
					while (s && mtlpath[s] != '/') --s;
					mtlpath[s + 1u] = '\0';
					string_append(mtlpath, mtlname, FILE_PATH_SIZE);
						
					read_mtl(mtlpath, model_info);
				}
			}
		}
		break;
			
		case 'o': // New Object
		{
			++it;

			it = line_jump_spaces(it);
				
			char delimiters[] = {
				' ',
				'\n',
				'\r'
			};
				
			u32 name_size = string_split(it, delimiters, 3u);
			if (name_size == 0u) {
					
				SV_LOG_ERROR("Can't read the object name in line %u", line_count + 1);
				corrupted = TRUE;
			}
			else {
					
				if (using_default) {
					using_default = FALSE;
				}
				else {
						
					mesh = new_obj_mesh(&meshes);
				}

				string_set(mesh->name, it, name_size, NAME_SIZE);
			}
		}
		break;
			
		case 's':
		{
			++it;
			it = line_jump_spaces(it);
			if (*it != '1')
				smooth = FALSE;
			else smooth = TRUE;
		}
		break;
			
		case 'g': // New Group
		{
			++it;
				
			it = line_jump_spaces(it);
				
			char delimiters[] = {
				' ',
				'\n',
				'\r'
			};
				
			u32 name_size = string_split(it, delimiters, 3u);
			if (name_size == 0u) {
					
				SV_LOG_ERROR("Can't read the object name in line %u", line_count + 1);
				corrupted = TRUE;
			}
			else {
					
				if (mesh->triangles.size == 0) {

					string_set(mesh->name, it, name_size, NAME_SIZE);
				}
				else {
						
					mesh = new_obj_mesh(&meshes);
				}
			}
		}
		break;
			
		case 'v': // Vertex info
		{
			++it;
					
			switch(*it) {
					
			case ' ': // Position
			{
				v3* v = array_add(&positions);

				b8 res;
				it = line_read_f32(it, &v->x, &res);
				if (res) it = line_read_f32(it, &v->y, &res);
				if (res) it = line_read_f32(it, &v->z, &res);
					
				if (!res) {
					corrupted = TRUE;
					SV_LOG_ERROR("Can't read the vector at line %u", line_count + 1);
				}
			}
			break;
				
			case 'n': // Normal
			{
				++it;
					
				v3* v = array_add(&normals);

				b8 res;
				it = line_read_f32(it, &v->x, &res);
				if (res) it = line_read_f32(it, &v->y, &res);
				if (res) it = line_read_f32(it, &v->z, &res);
			
				if (!res) {
					corrupted = TRUE;
					SV_LOG_ERROR("Can't read the vector at line %u", line_count + 1);
				}
			}
			break;
			
			case 't': // Texcoord
			{
				++it;

				v2* v = array_add(&texcoords);

				b8 res;

				it = line_read_f32(it, &v->x, &res);
				if (!res) {
					corrupted = TRUE;
					SV_LOG_ERROR("Can't read the vector at line %u", line_count + 1);
				}

				// v coord is optional
				it = line_read_f32(it, &v->y, NULL);

				v->y = 1.f - v->y;
			}
			break;
		    
			}		    
		}
		break;
		
		case 'f': // Face
		{
			++it;
			it = line_jump_spaces(it);

			u32 vertex_count = 0u;
		    
			i32 position_index[OBJ_POLYGON_COUNT];
			i32 normal_index[OBJ_POLYGON_COUNT];
			i32 texcoord_index[OBJ_POLYGON_COUNT];

			memory_zero(position_index, sizeof(i32) * OBJ_POLYGON_COUNT);
			memory_zero(normal_index, sizeof(i32) * OBJ_POLYGON_COUNT);
			memory_zero(texcoord_index, sizeof(i32) * OBJ_POLYGON_COUNT);

			char delimiters[] = {
				' ',
				'/'
			};

			b8 res;

			foreach(i, OBJ_POLYGON_COUNT) {
				
				it = line_read_i32(it, position_index + i, delimiters, 2u, &res);
				if (res && *it == '/') {

					++it;

					if (*it == '/') {
						it = line_read_i32(it, normal_index + i, delimiters, 2u, &res);
					}
					else {
						it = line_read_i32(it, texcoord_index + i, delimiters, 2u, &res);

						if (res && *it == '/') {
							++it;
							it = line_read_i32(it, normal_index + i, delimiters, 2u, &res);
						}
					}
				}

				if (!res) break;

				++vertex_count;

				it = line_jump_spaces(it);
				if (*it == '\r' || *it == '\n') break;
			}

			if (*it != '\r' && *it != '\n') {
				SV_LOG_ERROR("Can't read more than %u vertices in one face. Try to triangulate this .obj. Line %u", OBJ_POLYGON_COUNT, line_count + 1);
			}

			if (!res || vertex_count < 3) {
				res = FALSE;
				SV_LOG_ERROR("Can't read the face at line %u", line_count + 1);
				corrupted = FALSE;
			}

			if (res) {
			    
				if (vertex_count == 3u) {

					ObjTriangle* t = array_add(&mesh->triangles);
					t->smooth = smooth;
			    
					foreach(i, 3u) {
				
						t->position_indices[i] = position_index[i];
						t->normal_indices[i] = normal_index[i];
						t->texcoord_indices[i] = texcoord_index[i];
					}
				}
				else if (vertex_count == 4u) {

					ObjTriangle* t0 = array_add(&mesh->triangles);
					t0->smooth = smooth;

					foreach(i, 3u) {

						t0->position_indices[i] = position_index[i];
						t0->normal_indices[i] = normal_index[i];
						t0->texcoord_indices[i] = texcoord_index[i];
					}

					ObjTriangle* t1 = array_add(&mesh->triangles);
					t1->smooth = smooth;

					foreach(j, 3u) {

						u32 i;
				    
						switch (j) {

						case 0:
							i = 0u;
							break;

						case 1:
							i = 2u;
							break;

						default:
							i = 3u;
							break;
					
						}
				
						t1->position_indices[j] = position_index[i];
						t1->normal_indices[j] = normal_index[i];
						t1->texcoord_indices[j] = texcoord_index[i];
					}
				}
			}			
		}
		break;
		
		}

		it = line_next(it);
	}

	if (corrupted) {
		return FALSE;
	}

	// Parse obj format to my own format
	{
		if (meshes.size > MODEL_INFO_MAX_MESHES) {
			SV_LOG_ERROR("The obj '%s' exceeds the mesh limit\n", filepath);
		}

		foreach(i, SV_MIN(meshes.size, MODEL_INFO_MAX_MESHES)) {

			ObjMesh* obj_mesh = (ObjMesh*)array_get(&meshes, i);
				
			if (obj_mesh->triangles.size == 0)
				continue;

			// Compute min and max position indices
			u32 min_position_index = u32_max;
			u32 max_position_index = 0u;
			foreach(ti, obj_mesh->triangles.size) {
				
				const ObjTriangle* t = (const ObjTriangle*)array_get(&obj_mesh->triangles, ti);
				
				foreach(j, 3u) {
			    
					i32 i = t->position_indices[j];
					assert(i != 0);

					if (i > 0) {
						--i;
					}
					else if (i < 0) {
						i = (i32)positions.size + i;
					}
					else i = min_position_index;

					min_position_index = SV_MIN((u32)i, min_position_index);
					max_position_index = SV_MAX((u32)i, max_position_index);
				}
			}

			MeshInfo* mesh = model_info->meshes + model_info->mesh_count++;

			if (obj_mesh->name[0])
				string_copy(mesh->name, obj_mesh->name, NAME_SIZE);

			mesh->material_index = obj_mesh->material_index;

			u32 elements = max_position_index - min_position_index + 1u;
			u32 index_count = obj_mesh->triangles.size * 3u;

			{
				u32 memory_size = elements * (sizeof(v3) + sizeof(v3) + sizeof(v2)) + index_count * sizeof(u32);
				mesh->_memory = memory_allocate(memory_size);

				mesh->positions = (v3*)mesh->_memory;
				mesh->normals = (v3*)(mesh->_memory + elements * (sizeof(v3)));
				mesh->texcoords = (v2*)(mesh->_memory + elements * (sizeof(v3) + sizeof(v3)));
				mesh->indices = (u32*)(mesh->_memory + elements * (sizeof(v3) + sizeof(v3) + sizeof(v2)));

				mesh->vertex_count = elements;
				mesh->index_count = index_count;
			}

			memory_copy(mesh->positions, positions.data + min_position_index, elements * sizeof(v3));

			const ObjTriangle* it0 = (const ObjTriangle*)obj_mesh->triangles.data;
			u32* it1 = mesh->indices;
			const ObjTriangle* end = it0 + obj_mesh->triangles.size;

			i32 max = (i32)positions.size;
			u32 min = min_position_index;

			while (it0 != end) {

				foreach(j, 3u) {
			    
					u32 index = parse_objindex_to_absolute(it0->position_indices[j], max) - min;
					u32 normal_index = parse_objindex_to_absolute(it0->normal_indices[j], max);
					u32 texcoord_index = parse_objindex_to_absolute(it0->texcoord_indices[j], max);

					// TODO: Handle errors

					v3* normal_dst = mesh->normals + index;
					v2* texcoord_dst = mesh->texcoords + index;

					v3* normal_src = (v3*)array_get(&normals, normal_index);
					v2* texcoord_src = (v2*)array_get(&texcoords, texcoord_index);
					
					*normal_dst = *normal_src;
					*texcoord_dst = *texcoord_src;
			
					*it1 = index;
					++it1;
				}

				++it0;
			}
		}
	}

	// Centralize meshes
	/*
	foreach(mi, model_info->meshes.size) {

		MeshInfo* mesh = (MeshInfo*)array_get(&model_info->meshes, mi);

		f32 min_x = 9999999999.f;
		f32 min_y = 9999999999.f;
		f32 min_z = 9999999999.f;
		f32 max_x = -9999999999.f;
		f32 max_y = -9999999999.f;
		f32 max_z = -9999999999.f;

		foreach(i, mesh->positions.size) {

			v3 pos = *(v3*)array_get(&mesh->positions, i);

			min_x = SV_MIN(min_x, pos.x);
			min_y = SV_MIN(min_y, pos.y);
			min_z = SV_MIN(min_z, pos.z);

			max_x = SV_MAX(max_x, pos.x);
			max_y = SV_MAX(max_y, pos.y);
			max_z = SV_MAX(max_z, pos.z);
		}

		v3 center = v3_set(min_x + (max_x - min_x) * 0.5f, min_y + (max_y - min_y) * 0.5f, min_z + (max_z - min_z) * 0.5f);
		f32 dim = SV_MAX(SV_MAX(max_x - min_x, max_y - min_y), max_z - min_z);

		foreach(i, mesh->positions.size) {

			v3* pos = (v3*)array_get(&mesh->positions, i);

			*pos = v3_sub(*pos, center);
			*pos = v3_div_scalar(*pos, dim);
		}

		mesh->transform_matrix = m4_mul(m4_translate_v3(center), m4_scale_f32(dim));
	}
	*/

	return TRUE;
}

///////////////////////////////////////////////////////////// FBX FORMAT ///////////////////////////////////////////////////
/*

static void read_fbx_node(Deserializer* s)
{
	u32 end_offset;
	u32 num_properties;
	u32 property_list_length;
	u8 name_length;
	char name[256];

	deserialize_u32(s, &end_offset);
	deserialize_u32(s, &num_properties);
	deserialize_u32(s, &property_list_length);
	deserialize_u8(s, &name_length);
	deserializer_read(s, name, name_length);
	name[name_length] = '\0';

	SV_LOG_INFO("Node: %s\n", name);

	// Properties
	foreach(i, num_properties) {

		char type_code;

		deserialize_char(s, &type_code);

		switch (type_code)
		{
		case 'Y':
		{
			i16 i;
			deserialize_i16(s, &i);
			SV_LOG_INFO("i16: %i\n", (i32)i);
		}
		break;

		case 'C':
		{
			b8 i;
			deserialize_b8(s, &i);
			SV_LOG_INFO("b8: %i\n", (i32)i);
		}
		break;

		case 'I':
		{
			i32 i;
			deserialize_i32(s, &i);
			SV_LOG_INFO("i32: %i\n", i);
		}
		break;

		case 'F':
		{
			f32 i;
			deserialize_f32(s, &i);
			SV_LOG_INFO("f32: %f\n", i);
		}
		break;

		case 'D':
		{
			f64 i;
			deserialize_f64(s, &i);
			SV_LOG_INFO("f64: %f\n", (f32)i);
		}
		break;

		case 'L':
		{
			i64 i;
			deserialize_i64(s, &i);
			SV_LOG_INFO("i64: %f\n", (i32)i);
		}
		break;

		case 'R':
		{
			u32 size;
			deserialize_u32(s, &size);
			char* data = memory_allocate(size);
			deserializer_read(s, data, size);
			memory_free(data);
		}
		break;

		case 'S':
		{
			u32 size;
			deserialize_u32(s, &size);
			char* data = memory_allocate(size + 1);
			deserializer_read(s, data, size);
			data[size] = '\0';
			SV_LOG_INFO("String: %s\n", data);
			memory_free(data);
		}
		break;

		case 'f':
		case 'd':
		case 'l':
		case 'i':
		case 'b':
		{
			u32 length;
			u32 encoding;
			u32 compressed_length;

			deserialize_u32(s, &length);
			deserialize_u32(s, &encoding);
			deserialize_u32(s, &compressed_length);

			if (encoding == 0) {
				SV_LOG_INFO("Sos re trol\n");
			}
			else if (encoding == 1) {

				mz_zip_archive zip;
				mz_zip_zero_struct(&zip);

				//mz_zip_read_archive_data(mz_zip_archive * pZip, mz_uint64 file_ofs, void* pBuf, size_t n);

				if (mz_zip_reader_init_mem(&zip, s->data + s->cursor, compressed_length, 0)) {

					mz_zip_reader_end(&zip);
				}
			}

			s->cursor += compressed_length;
		}
		break;

		deafault:
			i = num_properties;
			SV_LOG_ERROR("Node '%s' corrupted\n", name);
			break;

		}
	}

	// Nested list
	while (s->cursor < end_offset) {
		
		read_fbx_node(s);
	}
}

static b8 model_load_fbx(ModelInfo* model_info, const char* filepath, char* it, u32 file_size)
{
	Deserializer s;
	deserializer_begin_buffer(&s, it, file_size);

	// Header
	{
		char magic[21];
		deserializer_read(&s, magic, 21);
		SV_LOG_INFO("Magic: %s\n", magic);
		
		s.cursor = 23;
		u32 version;
		deserialize_u32(&s, &version);
		SV_LOG_INFO("Version: %u\n", version);
	}

	while (s.cursor < file_size - 13) {

		read_fbx_node(&s);
	}
	
	deserializer_end_buffer(&s);
	return TRUE;
}
*/

////////////////////////////////////////// DAE FORMAT /////////////////////////////////////

#define DAE_MAX_NODES 200

typedef struct DaeNode DaeNode;

typedef enum {
	DaeNodeType_Unknown,
	DaeNodeType_Mesh,
	DaeNodeType_Joint
} DaeNodeType;

struct DaeNode {
	DaeNodeType type;
	m4 local_matrix;
	m4 global_matrix; // It's not transformed by the local matrix
	DaeNode* parent;
};

typedef struct {
	DaeNode node;
	char name[NAME_SIZE];
} DaeJointInfo;

typedef struct {
	f32 time_stamp;
	JointPoseInfo* poses;
	u32 pose_count;
	u32 pose_size;
} DaeKeyFrameInfo;

typedef struct {
	char name[NAME_SIZE];
	DaeKeyFrameInfo keyframes[MODEL_INFO_MAX_KEYFRAMES];
	u32 keyframe_count;
} DaeAnimationInfo;

typedef struct {

	DaeNode node;

	char name[NAME_SIZE];
	char geometry_id[100];
	char controller_id[100];

	u8* _memory;

	v3* positions;
	v3* normals;
	v2* texcoords;

	u32 position_count;
	u32 normal_count;
	u32 texcoord_count;

	u32* indices;
	u32 index_count; // Thats the real mesh index count. xml_index_count = index_count * index_stride
	u32 index_stride;

	u8* _animation_memory;

	WeightInfo* weights; // Length of position_count

	m4 bind_matrix;
	u32 material_index;

} DaeMeshInfo;

typedef struct {
	DaeMeshInfo meshes[MODEL_INFO_MAX_MESHES];
	u32 mesh_count;

	DaeJointInfo joints[MODEL_INFO_MAX_JOINTS];
	u32 joint_count;

	DaeAnimationInfo animations[MODEL_INFO_MAX_ANIMATIONS];
	u32 animation_count;
} DaeModelInfo;

inline const char* dae_read_f32(const char* it, f32* value, b8* res)
{
	*value = 0.f;
	it = line_jump_spaces(it);

	if (res)* res = FALSE;
	const char* start = it;

	if (*it == '-') ++it;

	u32 dots = 0u;

	while (*it != '\0' && *it != ' ' && *it != '\n' && *it != '\r' && *it != '<' && *it != 'e') {
		if (!char_is_number(*it) && *it != '.')
			return start;

		if (*it == '.')
			++dots;

		++it;
	}

	if (it == start || dots > 1) return start;

	u32 size = it - start;

	char value_str[20u];
	memory_copy(value_str, start, size);
	value_str[size] = '\0';
	// TODO: Use my own function
	*value = (f32)atof(value_str);

	if (*it == 'e') {
		++it;
		i32 v;

		const char* delimiters = " >";

		b8 r;
		it = line_read_i32(it, &v, delimiters, string_size(delimiters), &r);
		if (r) {

			*value *= pow(10.f, v);
		}
		else return start;
	}

	if (res) *res = TRUE;

	return it;
}

inline b8 dae_read_float_buffer(const char* begin, const char* end, f32* dst, u32 count, u32 stride)
{
	b8 res = TRUE;
	u32 i = 0;
	const char* c = begin;
	while (i < count && c < end && res) {

		u32 j = 0;

		do {
			c = dae_read_f32(c, dst++, &res);
		} while (++j < stride && res);

		++i;
	}

	if (i != count) {
		res = FALSE;
	}

	return res;
}

b8 dae_read_uint_buffer(const char* begin, const char* end, u32* dst, u32 count)
{
	char delimiters[] = {
		'<', ' '
	};

	b8 res = TRUE;
	u32 i = 0;
	const char* c = begin;
	while (i < count && c < end && res) {

		u32 j = 0;
		c = line_read_u32(c, dst++, delimiters, SV_ARRAY_SIZE(delimiters), &res);
		++i;
	}

	return res;
}

static b8 dae_load_geometry(DaeModelInfo* model_info, XMLElement root, const char* filepath)
{
	XMLElement geometries = root;

	if (xml_enter_child(&geometries, "library_geometries")) {

		XMLElement geo = geometries;

		if (xml_enter_child(&geo, "geometry")) {
			do {

				DaeMeshInfo* mesh;
				{
					if (model_info->mesh_count >= MODEL_INFO_MAX_MESHES) {
						SV_LOG_ERROR("The dae file '%s' exceeds the mesh limit\n", filepath);
						break;
					}
					
					mesh = model_info->meshes + model_info->mesh_count++;
				}

				if (!xml_get_attribute(&geo, mesh->name, NAME_SIZE, "name")) {
					SV_LOG_ERROR("A geometry element haven't name\n");
					continue;
				}
				if (!xml_get_attribute(&geo, mesh->geometry_id, 100, "id")) {
					SV_LOG_ERROR("A geometry element haven't id\n");
					continue;
				}
				u32 mesh_name_size = string_size(mesh->name);

				SV_LOG_INFO("Mesh: %s\n", mesh->name);

				mesh->node.type = DaeNodeType_Mesh;
				mesh->node.local_matrix = m4_identity();
				mesh->node.global_matrix = m4_identity();
				mesh->bind_matrix = m4_identity();
				mesh->material_index = u32_max;

				XMLElement xml_mesh = geo;

				if (xml_enter_child(&xml_mesh, "mesh")) {

					XMLElement src = xml_mesh;

					const char* begin_position = NULL;
					const char* begin_normal = NULL;
					const char* begin_texcoord = NULL;
					const char* begin_index = NULL;

					const char* end_position = NULL;
					const char* end_normal = NULL;
					const char* end_texcoord = NULL;
					const char* end_index = NULL;

					u32 position_count = 0;
					u32 normal_count = 0;
					u32 texcoord_count = 0;
					u32 index_count = 0;

					// TODO: Should compute this value from xml
					u32 index_stride = 3;

					if (xml_enter_child(&src, "source")) {

						do {

							char src_name[100];
							if (xml_get_attribute(&src, src_name, 100, "id")) {

								u32 src_name_size = string_size(src_name);
								if (src_name_size <= mesh_name_size) {
									SV_LOG_ERROR("Some geometry naming is wrong\n");
									continue;
								}

								const char* n = src_name + mesh_name_size;

								// Positions
								if (string_equals(n, "-mesh-positions")) {

									XMLElement floats = src;
									if (xml_enter_child(&floats, "float_array")) {

										u32 count;
										if (xml_get_attribute_u32(&floats, &count, "count")) {

											if (xml_element_content(&floats, &begin_position, &end_position)) {
												position_count = count / 3;
											}
										}
									}
								}

								// Normals
								if (string_equals(n, "-mesh-normals")) {

									XMLElement floats = src;
									if (xml_enter_child(&floats, "float_array")) {

										u32 count;
										if (xml_get_attribute_u32(&floats, &count, "count")) {

											if (xml_element_content(&floats, &begin_normal, &end_normal)) {
												normal_count = count / 3;
											}
										}
									}
								}

								// Texcoord
								if (string_equals(n, "-mesh-map-0")) {

									XMLElement floats = src;
									if (xml_enter_child(&floats, "float_array")) {

										u32 count;
										if (xml_get_attribute_u32(&floats, &count, "count")) {

											if (xml_element_content(&floats, &begin_texcoord, &end_texcoord)) {
												texcoord_count = count / 2;
											}
										}
									}
								}
							}

						} while (xml_next(&src));
					}

					// Indices
					{
						XMLElement xml_indices = xml_mesh;
						if (xml_enter_child(&xml_indices, "triangles")) {

							u32 count;
							if (xml_get_attribute_u32(&xml_indices, &count, "count")) {

								if (xml_enter_child(&xml_indices, "p")) {

									if (xml_element_content(&xml_indices, &begin_index, &end_index)) {
										index_count = count * 3;
									}
								}
							}
						}
					}

					// Construct mesh
					if (position_count && normal_count && texcoord_count && index_count) {

						// Allocate dae memory
						{
							u32 size = position_count * sizeof(v3);
							size += normal_count * sizeof(v3);
							size += texcoord_count * sizeof(v2);
							size += index_count * index_stride * sizeof(u32);

							mesh->_memory = memory_allocate(size);

							mesh->positions = (v3*)mesh->_memory;
							mesh->normals = (v3*)(mesh->_memory + position_count * sizeof(v3));
							mesh->texcoords = (v2*)(mesh->_memory + position_count * sizeof(v3) + normal_count * sizeof(v3));
							mesh->indices = (u32*)(mesh->_memory + position_count * sizeof(v3) + normal_count * sizeof(v3) + texcoord_count * sizeof(v2));

							mesh->position_count = position_count;
							mesh->normal_count = normal_count;
							mesh->texcoord_count = texcoord_count;
							mesh->index_count = index_count;
							mesh->index_stride = index_stride;
						}

						// Fill xml vertex data
						{
							b8 res = dae_read_float_buffer(begin_position, end_position, (f32*)mesh->positions, position_count, 3);

							if (!res) {
								SV_LOG_ERROR("Can't read position data properly\n");
							}

							res = dae_read_float_buffer(begin_normal, end_normal, (f32*)mesh->normals, normal_count, 3);

							if (!res) {
								SV_LOG_ERROR("Can't read normal data properly\n");
							}

							res = dae_read_float_buffer(begin_texcoord, end_texcoord, (f32*)mesh->texcoords, texcoord_count, 2);

							if (!res) {
								SV_LOG_ERROR("Can't read texcoord data properly\n");
							}

							res = dae_read_uint_buffer(begin_index, end_index, (u32*)mesh->indices, index_count * index_stride);

							if (!res) {
								SV_LOG_ERROR("Can't read index data properly\n");
							}
						}
					}
					else {
						SV_LOG_ERROR("Can't load the mesh '%s'\n", mesh->name);
					}
				}

			} while (xml_next(&geo));
		}
	}
	else {
		SV_LOG_ERROR("Library geometries not found in .dae\n");
		return FALSE;
	}

	return TRUE;
}

static void dae_load_node(DaeModelInfo* model_info, XMLElement node, DaeNode* parent, m4 parent_matrix)
{
	char type_name[NAME_SIZE];
	if (!xml_get_attribute(&node, type_name, NAME_SIZE, "type")) {
		SV_LOG_ERROR("Can't read the type of a node\n");
		return;
	}

	DaeNode* current = NULL;

	m4 matrix = m4_identity();
	m4 global_matrix = parent_matrix;

	{
		XMLElement xml_matrix = node;
		if (xml_enter_child(&xml_matrix, "matrix")) {

			const char* begin;
			const char* end;

			if (xml_element_content(&xml_matrix, &begin, &end)) {

				if (!dae_read_float_buffer(begin, end, (f32*)& matrix, 16, 1)) {
					SV_LOG_ERROR("Can't read matrix info\n");
				}
			}
		}
	}

	if (string_equals("NODE", type_name)) {

		DaeMeshInfo* mesh = NULL;

		{
			char id[100];
			u32 type = 0;
			XMLElement xml = node;

			if (xml_enter_child(&xml, "instance_geometry")) {

				if (!xml_get_attribute(&xml, id, 100, "url")) {
					SV_LOG_ERROR("Can't read geometry id\n");
				}
				else type = 1;
			}
			else if (xml_enter_child(&xml, "instance_controller")) {

				if (!xml_get_attribute(&xml, id, 100, "url")) {
					SV_LOG_ERROR("Can't read controller id\n");
				}
				else type = 2;
			}

			const char* c = id;

			if (type != 0 && *c == '#') {
				++c;
			}

			if (type == 1) {

				foreach(i, model_info->mesh_count) {

					DaeMeshInfo* m = model_info->meshes + i;

					if (string_equals(m->geometry_id, c)) {

						mesh = m;
						break;
					}
				}
			}
			else if (type == 2) {

				foreach(i, model_info->mesh_count) {

					DaeMeshInfo* m = model_info->meshes + i;

					if (string_equals(m->controller_id, c)) {

						mesh = m;
						break;
					}
				}
			}

			if (mesh) {
				current = &mesh->node;
			}
		}
	}
	else if (string_equals("JOINT", type_name)) {

		char name[NAME_SIZE];
		if (!xml_get_attribute(&node, name, NAME_SIZE, "sid")) {
			SV_LOG_ERROR("Can't get the join name\n");
		}
		else {

			DaeJointInfo* joint = NULL;

			foreach(i, model_info->joint_count) {

				DaeJointInfo* j = model_info->joints + i;
				if (string_equals(j->name, name)) {
					joint = j;
					break;
				}
			}

			if (joint == NULL) {
				SV_LOG_ERROR("Joint '%s' not found in controller\n", name);
			}
			else {
				current = &joint->node;
			}
		}
	}
	else SV_LOG_ERROR("Unknown node type '%s'\n", type_name);

	if (current) {
		current->local_matrix = matrix;
		current->global_matrix = global_matrix;
		current->parent = parent;
	}

	// Childs
	XMLElement child = node;
	if (xml_enter_child(&child, "node")) {
		do {
			dae_load_node(model_info, child, current, m4_mul(global_matrix, matrix));
		} while (xml_next(&child));
	}
}

static b8 dae_load_nodes(DaeModelInfo* model_info, XMLElement root)
{
	XMLElement scene = root;

	if (xml_enter_child(&scene, "library_visual_scenes") && xml_enter_child(&scene, "visual_scene")) {

		XMLElement node = scene;

		if (xml_enter_child(&node, "node")) {
			do {

				dae_load_node(model_info, node, NULL, m4_identity());
			} 
			while (xml_next(&node));
		}
	}
	else {
		SV_LOG_ERROR("Visual scenes tag not found\n");
		return FALSE;
	}

	return TRUE;
}

static b8 dae_load_controllers(DaeModelInfo* model_info, XMLElement root)
{
	XMLElement controller = root;

	if (xml_enter_child(&controller, "library_controllers") && xml_enter_child(&controller, "controller")) {
		do {

			char controller_id[100];
			if (!xml_get_attribute(&controller, controller_id, 100, "id")) {
				SV_LOG_ERROR("Can't find the controller id\n");
				continue;
			}

			u32 controller_id_size = string_size(controller_id);
			
			XMLElement skin = controller;

			if (xml_enter_child(&skin, "skin")) {
				do {

					char source_str[100];
					if (!xml_get_attribute(&skin, source_str, 100, "source")) {
						SV_LOG_ERROR("Can't read source of an skin\n");
						continue;
					}

					DaeMeshInfo* mesh = NULL;

					u32 weight_count;
					const char* begin_weights;
					const char* end_weights;
					u32 vcount;
					const char* begin_vcount;
					const char* end_vcount;
					const char* begin_v;
					const char* end_v;

					// Find mesh
					{
						char* mesh_id = source_str;

						if (*mesh_id == '#')
							++mesh_id;

						foreach(i, model_info->mesh_count) {

							DaeMeshInfo* m = model_info->meshes + i;
							if (string_equals(m->geometry_id, mesh_id)) {
								mesh = m;
								break;
							}
						}

						if (mesh == NULL) {
							SV_LOG_ERROR("Skin source not found '%s'\n", source_str);
							continue;
						}
					}

					// Set controller id
					string_copy(mesh->controller_id, controller_id, 100);

					// bind_shape_matrix 
					{
						m4 bind_shape_matrix = m4_identity();
						{
							XMLElement xml = skin;
							if (xml_enter_child(&xml, "bind_shape_matrix")) {

								const char* begin;
								const char* end;

								if (xml_element_content(&xml, &begin, &end)) {
									dae_read_float_buffer(begin, end, (f32*)& bind_shape_matrix, 16, 1);
								}
							}
						}

						mesh->bind_matrix = bind_shape_matrix;
					}

					{
						XMLElement src = skin;
						if (xml_enter_child(&src, "source")) {
							do {
								char id[100];
								if (!xml_get_attribute(&src, id, 100, "id")) {
									SV_LOG_ERROR("Can't get the source id in the controller\n");
									continue;
								}

								if (controller_id_size > string_size(id) + 3) {
									SV_LOG_ERROR("The source id in the controller is wrong\n");
									continue;
								}

								const char* c = id + controller_id_size;

								if (string_equals(c, "-weights")) {

									XMLElement xml = src;
									if (xml_enter_child(&xml, "float_array")) {

										u32 count;
										if (!xml_get_attribute_u32(&xml, &count, "count")) {
											SV_LOG_ERROR("Can't get the weight count\n");
										}
										else {

											if (!xml_element_content(&xml, &begin_weights, &end_weights)) {
												SV_LOG_ERROR("Can't get the weights content\n");
											}
											else {
												weight_count = count;
											}
										}
									}
									else {
										SV_LOG_ERROR("Can't find the weights data\n");
									}
								}
								else if (string_equals(c, "-joints")) {

									XMLElement xml = src;
									if (xml_enter_child(&xml, "Name_array")) {

										u32 count;
										if (!xml_get_attribute_u32(&xml, &count, "count")) {
											SV_LOG_ERROR("Can't get the joint count\n");
										}
										else {

											const char* begin;
											const char* end;

											if (!xml_element_content(&xml, &begin, &end)) {
												SV_LOG_ERROR("Can't get the joint name content\n");
											}
											else {
												
												u32 i = 0;
												const char* c = begin;

												if (model_info->joint_count + count > MODEL_INFO_MAX_JOINTS) {
													SV_LOG_ERROR("The joint count limit is %u\n", MODEL_INFO_MAX_JOINTS);
													count = MODEL_INFO_MAX_JOINTS - model_info->joint_count;
												}

												while (i < count && c < end) {

													u32 size = string_split(c, " <", 2);
													
													u32 name_size = SV_MIN(size, NAME_SIZE);

													if (size != name_size) {
														SV_LOG_ERROR("The joint name limit is %u\n", NAME_SIZE);
													}
													
													DaeJointInfo joint;
													string_set(joint.name, c, name_size, NAME_SIZE);

													// Try to add
													{
														b8 repeated = FALSE;
														foreach(i, model_info->joint_count) {
															if (string_equals(model_info->joints[i].name, joint.name)) {
																repeated = TRUE;
															}
														}

														if (!repeated) {

															joint.node.type = DaeNodeType_Joint;
															joint.node.local_matrix = m4_identity();
															joint.node.global_matrix = m4_identity();
															joint.node.parent = NULL;

															model_info->joints[model_info->joint_count++] = joint;
														}
													}

													c += size;
													c = line_jump_spaces(c);
												}
											}
										}
									}
									else {
										SV_LOG_ERROR("Can't find the weights data\n");
									}
								}

							} while (xml_next(&src));
						}
					}

					// Vertex weights
					{
						XMLElement xml = skin;
						if (xml_enter_child(&xml, "vertex_weights")) {

							if (xml_get_attribute_u32(&xml, &vcount, "count")) {

								XMLElement e = xml;
								if (!(xml_enter_child(&e, "vcount") && xml_element_content(&e, &begin_vcount, &end_vcount))) {
									SV_LOG_ERROR("Can't get vertex weights in skin '%s'\n", mesh->name);
								}
								e = xml;
								if (!(xml_enter_child(&e, "v") && xml_element_content(&e, &begin_v, &end_v))) {
									SV_LOG_ERROR("Can't get vertex weights in skin '%s'\n", mesh->name);
								}
							}
							else {
								SV_LOG_ERROR("Can't get the vertex weight count in skin '%s'\n", mesh->name);
							}
						}
						else {
							SV_LOG_ERROR("Vertex weights in skin '%s' not found\n", mesh->name);
						}
					}

					b8 has_animation_data = weight_count > 0 && vcount > 0 && vcount == mesh->position_count;

					if (has_animation_data) {

						f32* weights = NULL;

						// Store animation memory
						{
							u32 size = vcount * sizeof(WeightInfo);
							size += weight_count * sizeof(f32);

							mesh->_animation_memory = memory_allocate(size);
							u8* it = mesh->_animation_memory;

							mesh->weights = (WeightInfo*)it;
							it += vcount * sizeof(WeightInfo);

							weights = (f32*)it;
						}

						// Read animation data
						{
							// Weights
							if (!dae_read_float_buffer(begin_weights, end_weights, weights, weight_count, 1)) {
								SV_LOG_ERROR("Can't read weight data properly\n");
							}

							if (vcount > 0 && vcount == mesh->position_count) {

								const char* count_it = begin_vcount;
								const char* it = begin_v;

								b8 res = TRUE;

								u32 index = 0;

								while (count_it < end_vcount && it < end_v) {

									u32 count;
									count_it = line_read_u32(count_it, &count, " <", 2, &res);
									if (res == FALSE)
										break;

									WeightInfo* info = mesh->weights + index;
									info->count = SV_MIN(count, MODEL_INFO_MAX_WEIGHTS);

									foreach(i, count) {

										u32 joint, weight;
										it = line_read_u32(it, &joint, " <", 2, &res);
										if (res == FALSE)
											break;

										it = line_read_u32(it, &weight, " <", 2, &res);
										if (res == FALSE)
											break;

										if (i < MODEL_INFO_MAX_WEIGHTS) {
											info->joint_indices[i] = joint;
											if (weight >= weight_count) info->weights[i] = 0.f;
											else info->weights[i] = weights[weight];
										}
									}

									if (res == FALSE)
										break;

									// TODO: Normalize it later
									// Normalize vertex
									{
										f32 len = 0.f;
										foreach(i, info->count) {
											len += info->weights[i];
										}

										f32 mul = 1.f / len;
										foreach(i, info->count) {
											info->weights[i] *= mul;
										}
									}

									++index;
								}

								if (res == FALSE) {
									SV_LOG_ERROR("Error parsing vertex weights in '%s' at index '%u'\n", mesh->name, index + 1);
								}
							}
						}
					}

				} while (xml_next(&skin));
			}

		} while (xml_next(&controller));
	}

	return TRUE;
}

static b8 dae_load_animations(DaeModelInfo* model_info, XMLElement root, const char* filepath)
{
	XMLElement animations = root;

	if (xml_enter_child(&animations, "library_animations") && xml_enter_child(&animations, "animation")) {
		do {

			char name[NAME_SIZE];
			if (!xml_get_attribute(&animations, name, NAME_SIZE, "name")) {
				SV_LOG_ERROR("Can't get an animation name\n");
				continue;
			}

			if (model_info->animation_count >= MODEL_INFO_MAX_ANIMATIONS) {
				SV_LOG_ERROR("The model '%s' exceeds the animation limit\n", filepath);
				return TRUE;
			}

			DaeAnimationInfo* animation = model_info->animations + model_info->animation_count++;
			string_copy(animation->name, name, NAME_SIZE);

			XMLElement xml = animations;

			u32 name_offset = string_size(animation->name) + string_size("_ArmatureAction_001_");
			u32 name_final_offset = string_size("_pose_matrix");

			if (xml_enter_child(&xml, "animation")) {
				do {

					char id[100];
					if (!xml_get_attribute(&xml, id, 100, "id")) {
						SV_LOG_ERROR("Can't get an animation id\n");
						continue;
					}

					u32 id_size = string_size(id);

					if (id_size <= name_offset + name_final_offset) {
						SV_LOG_ERROR("The id is invalid '%s'\n", id);
					}
					else {

						char joint_name[NAME_SIZE];
						{
							u32 size = id_size - name_offset - name_final_offset;
							string_set(joint_name, id + name_offset, size, NAME_SIZE);
						}

						DaeJointInfo* joint = NULL;
						u32 joint_index;

						foreach(i, model_info->joint_count) {
							DaeJointInfo* j = model_info->joints + i;
							if (string_equals(j->name, joint_name)) {
								joint = j;
								joint_index = i;
								break;
							}
						}

						if (joint == NULL) {
							SV_LOG_ERROR("Joint '%s' not found in '%s'\n", joint_name, filepath);
						}
						else {

							u32 count = 0;

							const char* begin_times = NULL;
							const char* end_times = NULL;

							const char* begin_matrices = NULL;
							const char* end_matrices = NULL;

							{
								XMLElement e = xml;
								if (xml_enter_child(&e, "source")) {
									do {

										char id[100];
										if (!xml_get_attribute(&e, id, 100, "id")) {
											SV_LOG_ERROR("Can't get the id source in joint '%s' from '%s'\n", joint->name, filepath);
										}
										else {

											u32 size = string_size(id);

											if (size <= 5) {
												SV_LOG_ERROR("Invalid id source in joint '%s' from '%s'\n", joint->name, filepath);
											}
											else {

												u32 type = 0;

												{
													const char* c = id + size - 5;
													if (string_equals(c, "input"))
														type = 1;
													else if (string_equals(c, "utput"))
														type = 2;
												}

												// Input
												if (type == 1) {
													XMLElement floats = e;
													if (xml_enter_child(&floats, "float_array")) {

														if (!xml_get_attribute_u32(&floats, &count, "count")) {
															SV_LOG_ERROR("Can't get the number of keyframes of joint '%s' in '%s'\n", joint->name, filepath);
															count = 0;
														}
														else {

															if (!xml_element_content(&floats, &begin_times, &end_times)) {
																SV_LOG_ERROR("Can't get the keyframes of joint '%s' in '%s'\n", joint->name, filepath);
															}
														}
													}
												}
												// Output
												else if (type == 2) {
													XMLElement floats = e;
													if (xml_enter_child(&floats, "float_array")) {

														u32 mcount = 0;

														if (!xml_get_attribute_u32(&floats, &mcount, "count")) {
															SV_LOG_ERROR("Can't get the number of keyframes matrices of joint '%s' in '%s'\n", joint->name, filepath);
														}
														else if (mcount / 16 != count) {
															SV_LOG_ERROR("Invalid keyframe matrix count in joint '%s' of '%s'\n", joint->name, filepath);
														}
														else {

															if (!xml_element_content(&floats, &begin_matrices, &end_matrices)) {
																SV_LOG_ERROR("Can't get the keyframes matrices of joint '%s' in '%s'\n", joint->name, filepath);
															}
														}
													}
												}
											}
										}

									} while (xml_next(&e));
								}
							}

							if (count && begin_times != NULL && begin_matrices != NULL) {

								b8 res;
								u32 i = 0;
								const char* c0 = begin_times;
								const char* c1 = begin_matrices;

								while (i < count && c0 < end_times && c1 < end_matrices) {

									f32 time;
									c0 = dae_read_f32(c0, &time, &res);
									if (!res)
										break;

									m4 matrix;
									foreach(i, 16) {
										c1 = dae_read_f32(c1, matrix.a + i, &res);
										if (!res)
											break;
									}

									// Register keyframe
									{
										// Find keyframe position
										u32 insert_index = animation->keyframe_count;
										b8 insert = TRUE;
										{
											foreach(i, animation->keyframe_count) {

												DaeKeyFrameInfo* key = animation->keyframes + i;

												if (fabs(key->time_stamp - time) < 0.00001f) {
													insert_index = i;
													insert = FALSE;
													break;
												}
												else if (key->time_stamp > time) {
													insert_index = i;
													break;
												}
											}
										}

										// Get keyframe memory
										DaeKeyFrameInfo* key;
										{
											if (insert) {

												if (animation->keyframe_count >= MODEL_INFO_MAX_KEYFRAMES) {
													SV_LOG_ERROR("The animation '%s' exceeds the keyframe count in '%s'\n", animation->name, filepath);
													key = animation->keyframes + MODEL_INFO_MAX_KEYFRAMES - 1;
												}
												else {
													if (insert_index == animation->keyframe_count) {

														key = animation->keyframes + animation->keyframe_count++;
													}
													else {

														for (i32 i = animation->keyframe_count; i > insert_index; --i) {
															animation->keyframes[i] = animation->keyframes[i - 1];
														}

														animation->keyframe_count++;
														key = animation->keyframes + insert_index;
														memory_zero(key, sizeof(DaeKeyFrameInfo));
													}
												}

												key->time_stamp = time;
											}
											else {
												key = animation->keyframes + insert_index;
											}
										}

										// Insert pose
										{
											JointPoseInfo* pose = NULL;

											// Dynamic reserve poses
											{
												if (key->pose_count >= key->pose_size) {

													u32 new_size = key->pose_size + 10;
													JointPoseInfo* new = memory_allocate(new_size * sizeof(JointPoseInfo));

													if (key->pose_count) {
														memory_copy(new, key->poses, key->pose_count * sizeof(JointPoseInfo));
														memory_free(key->poses);
													}

													key->poses = new;
													key->pose_size = new_size;
												}

												pose = key->poses + key->pose_count++;
											}

											pose->joint = joint_index;
											
											// Blender exports the joint information in his coordinate system
											//matrix = m4_mul(m4_rotate_x(-PI * 0.5f), matrix);

											pose->position = m4_decompose_position(matrix);
											pose->rotation = m4_decompose_rotation(matrix);
										}
									}

									if (!res)
										break;

									++i;
								}

								if (!res) {
									SV_LOG_ERROR("Can't parse properly the animation data of '%s' in '%s'\n", joint->name, filepath);
								}
							}
						}
					}

				} while (xml_next(&xml));
			}

		} while (xml_next(&animations));
	}

	return TRUE;
}

inline JointInfo* dae_add_to_hierarchy(ModelInfo* model_info, DaeModelInfo* dae_model_info, DaeJointInfo* joint)
{
	u32 index = joint - dae_model_info->joints;
	JointInfo* n = model_info->joints + index;

	model_info->hierarchy[model_info->hierarchy_count++] = n;

	return n;
}

static dae_add_in_hierarchy_nodes_with_parent(ModelInfo* model_info, DaeModelInfo* dae_model_info, DaeJointInfo* node0, JointInfo* node1)
{
	u32 child_count = 0;

	foreach(i, dae_model_info->joint_count) {

		DaeJointInfo* joint = dae_model_info->joints + i;

		if ((DaeJointInfo*)(joint->node.parent) == node0) {

			JointInfo* n = dae_add_to_hierarchy(model_info, dae_model_info, joint);
			dae_add_in_hierarchy_nodes_with_parent(model_info, dae_model_info, joint, n);
			child_count += n->child_count + 1;
		}
	}

	if (node0) {
		node1->child_count = child_count;
	}
}

static b8 model_load_dae(ModelInfo* model_info, const char* filepath, char* it, u32 file_size)
{
	DaeModelInfo* dae_model_info = memory_allocate(sizeof(DaeModelInfo));

	XMLElement root = xml_begin(it, file_size);

	if (root.corrupted) {
		SV_LOG_ERROR("Corrupted .dae\n");
		return FALSE;
	}

	if (!dae_load_geometry(dae_model_info, root, filepath)) {
		SV_LOG_ERROR("Can't load dae geometry '%s'\n", filepath);
		return FALSE;
	}

	if (!dae_load_controllers(dae_model_info, root)) {
		SV_LOG_ERROR("Can't load dae controllers '%s'\n", filepath);
		return FALSE;
	}

	if (!dae_load_nodes(dae_model_info, root)) {
		SV_LOG_ERROR("Can't load dae nodes '%s'\n", filepath);
		return FALSE;
	}

	if (!dae_load_animations(dae_model_info, root, filepath)) {
		SV_LOG_ERROR("Can't load dae animations '%s'\n", filepath);
		return FALSE;
	}

	// Parse dae info to general info
	{
		typedef struct DaeIndex DaeIndex;
		struct DaeIndex {
			u64 hash;
			u32 position;
			u32 normal;
			u32 texcoord;
			u32 index;
		};

		model_info->mesh_count = dae_model_info->mesh_count;
		model_info->joint_count = dae_model_info->joint_count;
		model_info->animation_count = dae_model_info->animation_count;

		// Meshes
		foreach(i, dae_model_info->mesh_count) {
			
			DaeMeshInfo* dae = dae_model_info->meshes + i;
			MeshInfo* mesh = model_info->meshes + i;

			mesh->material_index = dae->material_index;
			string_copy(mesh->name, dae->name, NAME_SIZE);

			// TODO: Reuse memory in diferent meshes
			u32 table_size = (u32)((f32)dae->index_count * 1.5f);
			DaeIndex* index_table = memory_allocate(table_size * sizeof(DaeIndex));

			foreach(i, dae->index_count) {

				DaeIndex index;
				index.position = dae->indices[i * dae->index_stride + 0];
				index.normal = dae->indices[i * dae->index_stride + 1];
				index.texcoord = dae->indices[i * dae->index_stride + 2];

				index.hash = (u64)index.position | ((u64)index.normal << 32);
				index.hash = hash_combine(index.hash, index.texcoord);

				DaeIndex* p;

				// Find in table
				{
					u32 table_index = index.hash % table_size;

					foreach(i, table_size) {

						u32 index0 = (table_index + i) % table_size;
						p = index_table + index0;

						if (p->hash == 0 || p->hash == index.hash) {
							break;
						}
					}

					assert_title(p != NULL, "Can't find a space in the dae vertex table");

					if (p != NULL && p->hash == 0) {

						*p = index;
						p->index = mesh->vertex_count++;
					}
				}

				if (p != NULL)
					dae->indices[i] = p->index;
			}

			// TODO: Not use indices if is necesary
			if (mesh->vertex_count == dae->index_count) {
				SV_LOG_WARNING("The mesh '%s' in model '%s' shouldn't use indexed rendering\n", dae->name, filepath);
			}

			// Reserve memory
			{
				u32 size = mesh->vertex_count * sizeof(v3) + dae->index_count * sizeof(u32);
				if (dae->normal_count) size += mesh->vertex_count * sizeof(v3);
				if (dae->texcoord_count) size += mesh->vertex_count * sizeof(v2);
				if (dae->weights) size += mesh->vertex_count * sizeof(WeightInfo);

				mesh->_memory = memory_allocate(size);

				u8* it = mesh->_memory;

				mesh->positions = (v3*)it;
				it += sizeof(v3) * mesh->vertex_count;

				if (dae->normal_count) {
					mesh->normals = (v3*)it;
					it += sizeof(v3) * mesh->vertex_count;
				}
				if (dae->texcoord_count) {
					mesh->texcoords = (v2*)it;
					it += sizeof(v2) * mesh->vertex_count;
				}
				if (dae->weights) {
					mesh->weights = (WeightInfo*)it;
					it += sizeof(WeightInfo) * mesh->vertex_count;
				}

				mesh->indices = (u32*)it;
			}

			// Set index data
			{
				memory_copy(mesh->indices, dae->indices, dae->index_count * sizeof(u32));
				mesh->index_count = dae->index_count;
			}

			// Set vertex data
			{
				m4 matrix = m4_mul(dae->node.local_matrix, dae->bind_matrix);
				matrix = m4_mul(dae->node.global_matrix, matrix);

				// Update positions
				{
					foreach(i, dae->position_count) {
						v4 p = v3_to_v4(dae->positions[i], 1.f);
						p = v4_transform(p, matrix);
						dae->positions[i] = v4_to_v3(p);
					}
				}
				// Positions
				foreach(i, table_size) {

					DaeIndex* index =  index_table + i;
					if (index->hash != 0)
						mesh->positions[index->index] = dae->positions[index->position];
				}

				// Normals
				if (dae->normal_count) {

					// Update normals
					{
						m4 m = matrix;
						m.v[0][3] = 0.f;
						m.v[1][3] = 0.f;
						m.v[2][3] = 0.f;
						m.v[3][3] = 1.f;

						m = m4_inverse(m);
						m = m4_transpose(m);

						foreach(i, dae->normal_count) {
							v4 p = v3_to_v4(dae->normals[i], 0.f);
							p = v4_transform(p, m);
							dae->normals[i] = v3_normalize(v4_to_v3(p));
						}
					}
					
					foreach(i, table_size) {

						DaeIndex* index = index_table + i;
						if (index->hash != 0)
							mesh->normals[index->index] = dae->normals[index->normal];
					}
				}

				// Texcoord
				if (dae->texcoord_count) {

					foreach(i, table_size) {

						DaeIndex* index = index_table + i;
						if (index->hash != 0)
							mesh->texcoords[index->index] = dae->texcoords[index->texcoord];
					}
				}

				// Weights
				if (dae->weights) {
					foreach(i, table_size) {

						DaeIndex* index = index_table + i;
						if (index->hash != 0)
							mesh->weights[index->index] = dae->weights[index->position];
					}
				}
			}

			memory_free(index_table);
		}

		// Joints
		foreach(i, dae_model_info->joint_count) {

			DaeJointInfo* dae = dae_model_info->joints + i;
			JointInfo* joint = model_info->joints + i;

			joint->local_matrix = dae->node.local_matrix;
			joint->inverse_bind_matrix = m4_inverse(m4_mul(dae->node.global_matrix, dae->node.local_matrix));
			string_copy(joint->name, dae->name, NAME_SIZE);

			// DEBUG
			SV_LOG_INFO("%s:\n", joint->name);
			m4 m = joint->inverse_bind_matrix;
			foreach(i, 4)
				SV_LOG_INFO("%f / %f / %f / %f\n", m.v[i][0], m.v[i][1], m.v[i][2], m.v[i][3]);
		}

		// Animations
		foreach(i, dae_model_info->animation_count) {

			DaeAnimationInfo* dae = dae_model_info->animations + i;
			AnimationInfo* anim = model_info->animations + i;

			string_copy(anim->name, dae->name, NAME_SIZE);

			// Compute pose buffer size
			u32 pose_size = 0;
			foreach(j, dae->keyframe_count) {
				DaeKeyFrameInfo* key = dae->keyframes + j;
				pose_size += key->pose_count;
			}

			anim->pose_memory_size = pose_size;

			if (pose_size == 0)
				continue;

			anim->_pose_memory = memory_allocate(pose_size * sizeof(JointPoseInfo));
			u8* it = anim->_pose_memory;

			// Set pose and keyframe data
			anim->keyframe_count = dae->keyframe_count;
			foreach(j, dae->keyframe_count) {
				DaeKeyFrameInfo* key0 = dae->keyframes + j;
				KeyFrameInfo* key1 = anim->keyframes + j;

				key1->time_stamp = key0->time_stamp;
				key1->pose_count = key0->pose_count;

				key1->poses = (JointPoseInfo*)it;
				it += key1->pose_count * sizeof(JointPoseInfo);

				foreach(w, key0->pose_count) {
					key1->poses[w] = key0->poses[w];
				}
			}
		}

		// Hierarchy
		{
			dae_add_in_hierarchy_nodes_with_parent(model_info, dae_model_info, NULL, NULL);
		}
	}

	// Free dae data
	{
		foreach(i, dae_model_info->mesh_count) {

			DaeMeshInfo* mesh = dae_model_info->meshes + i;

			if (mesh->_memory)
				memory_free(mesh->_memory);

			if (mesh->_animation_memory)
				memory_free(mesh->_animation_memory);
		}

		foreach(i, dae_model_info->animation_count) {

			DaeAnimationInfo* anim = dae_model_info->animations + i;

			foreach(j, anim->keyframe_count) {
				DaeKeyFrameInfo* key = anim->keyframes + j;
				if (key->poses)
					memory_free(key->poses);
			}
		}

		memory_free(dae_model_info);
	}

	return TRUE;
}

b8 import_model(ModelInfo* model_info, const char* filepath)
{
	const char* model_extension = filepath_extension(filepath);

	if (model_extension == NULL) {
		SV_LOG_ERROR("The filepath '%s' isn't a valid model filepath", filepath);
		return FALSE;
	}

	u32 model_format = u32_max;

	{
		const char* supported_formats[] = {
			".obj",
			".dae"
		};

		foreach(i, SV_ARRAY_SIZE(supported_formats)) {

			if (string_equals(model_extension, supported_formats[i])) {
				model_format = i;
				break;
			}
		}

		if (model_format == u32_max) {
			SV_LOG_ERROR("Model format '%s' not supported", model_extension);
			return FALSE;
		}
	}

	memory_zero(model_info, sizeof(ModelInfo));

	// Get folder path
	{
		const char* name = filepath_name(filepath);
		u32 folderpath_size = name - filepath;
		memory_copy(model_info->folderpath, filepath, folderpath_size);
		model_info->folderpath[folderpath_size] = '\0';
	}

	b8 res = TRUE;
	
	// Load file
	u8* file_data = NULL;
	u32 file_size = 0;
	if (file_read_binary(filepath, &file_data, &file_size)) {
		
		switch (model_format)
		{
		case 0:
			res = model_load_obj(model_info, filepath, file_data);
			break;
		case 1:
			res = model_load_dae(model_info, filepath, file_data, file_size);
			break;
		}
	}
	else {
		SV_LOG_ERROR("Can't load the model '%s', not found", filepath);
		res = FALSE;
	}

	if (file_data) {
		memory_free(file_data);
	}

	// DEBUG: Print animation data
	{
		foreach(a, model_info->animation_count) {

			AnimationInfo* animation = model_info->animations + a;

			SV_LOG_INFO("Animation %s\n", animation->name);

			foreach(i, animation->keyframe_count) {
				KeyFrameInfo* key = animation->keyframes + i;

				SV_LOG_INFO("\tKeyframe %u: %f\n", i + 1, key->time_stamp);

				foreach(j, key->pose_count) {

					v3 p = key->poses[j].position;
					v4 r = key->poses[j].rotation;

					SV_LOG_INFO("\t\tJoint: %u\n", key->poses[j].joint);
					SV_LOG_INFO("\t\tPosition: %f, %f, %f\n", p.x, p.y, p.z);
					SV_LOG_INFO("\t\tRotation: %f, %f, %f, %f\n", r.x, r.y, r.z, r.w);
				}

				SV_LOG_INFO("\n");
			}
		}
	}

	// DEBUG: Print hierarchy data
	{
		i32 level = 0;
		foreach(i, model_info->hierarchy_count) {

			JointInfo* joint = model_info->hierarchy[i];
			SV_LOG_INFO("Joint %u: %s\n", joint->child_count, joint->name);
		}
	}

	if (!res) {
		free_model_info(model_info);
	}

	return res;
}

void free_model_info(ModelInfo* model_info)
{
	foreach(i, model_info->mesh_count) {
		MeshInfo* mesh = model_info->meshes + i;

		if (mesh->_memory)
			memory_free(mesh->_memory);
	}

	foreach(i, model_info->animation_count) {
		AnimationInfo* anim = model_info->animations + i;

		if (anim->_pose_memory)
			memory_free(anim->_pose_memory);
	}

	memory_zero(model_info, sizeof(ModelInfo));
}
