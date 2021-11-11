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

///////////////////////////////////////////// MESH LOADING /////////////////////////////////////////////

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

		MaterialInfo* mat = array_add(&model_info->materials);
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
							mat = array_add(&model_info->materials);
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
						
					foreach(i, model_info->materials.size) {
							
						const MaterialInfo* m = (const MaterialInfo*)array_get(&model_info->materials, i);
							
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
		foreach(i, meshes.size) {

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

			MeshInfo* mesh = array_add(&model_info->meshes);
			mesh->positions = array_init(v3, 2.f);
			mesh->normals = array_init(v3, 2.f);
			mesh->texcoords = array_init(v2, 2.f);
			mesh->indices = array_init(u32, 2.f);

			if (obj_mesh->name[0])
				string_copy(mesh->name, obj_mesh->name, NAME_SIZE);

			mesh->material_index = obj_mesh->material_index;

			u32 elements = max_position_index - min_position_index + 1u;

			array_resize(&mesh->positions, elements);
			array_resize(&mesh->normals, elements);
			array_resize(&mesh->texcoords, elements);

			array_resize(&mesh->indices, obj_mesh->triangles.size * 3u);

			memory_copy(mesh->positions.data, positions.data + min_position_index, elements * sizeof(v3));

			const ObjTriangle* it0 = (const ObjTriangle*)obj_mesh->triangles.data;
			u32* it1 = (u32*)mesh->indices.data;
			const ObjTriangle* end = it0 + obj_mesh->triangles.size;

			i32 max = (i32)positions.size;
			u32 min = min_position_index;

			while (it0 != end) {

				foreach(j, 3u) {
			    
					u32 index = parse_objindex_to_absolute(it0->position_indices[j], max) - min;
					u32 normal_index = parse_objindex_to_absolute(it0->normal_indices[j], max);
					u32 texcoord_index = parse_objindex_to_absolute(it0->texcoord_indices[j], max);

					// TODO: Handle errors

					v3* normal_dst = (v3*)array_get(&mesh->normals, index);
					v2* texcoord_dst = (v2*)array_get(&mesh->texcoords, index);

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

		mesh->transform_matrix = mat4_multiply(mat4_translate_v3(center), mat4_scale_f32(dim));
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

		default:
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

typedef struct {

	b8 corrupted;
	const char* data;
	u32 size;

	i32 level;
	const char* begin;
	const char* end;

} XMLElement;

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

static XMLElement xml_begin(const char* data, u32 size)
{
	XMLElement e;
	e.data = data;
	e.size = size;
	e.corrupted = FALSE;
	e.level = 0;

	const char*c = line_jump_spaces(e.data);

	while (!e.corrupted) {

		if (*c== '<') {

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

static u32 xml_name(const XMLElement* e, char* buffer, u32 buffer_size)
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

inline b8 xml_string_equals_to_normal(const char* xml_str, const char* normal)
{
	while (*xml_str != '\0' && *normal != '\0') {

		if (*xml_str == '/' || *xml_str == '>' || *xml_str == ' ') {
			xml_str = "";
		}

		if (*xml_str != *normal) {
			return FALSE;
		}

		++xml_str;
		++normal;
	}
	return TRUE;
}

static b8 xml_enter_child(XMLElement* e, const char* name)
{
	const char* c = xml_exit_tag(e->begin);

	while (1) {

		while (*c != '\0' && *c != '<') {
			++c;
		}

		if (*c == '\0') {
			e->corrupted = TRUE;
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

static b8 xml_next(XMLElement* e)
{

}

static b8 model_load_dae(ModelInfo* model_info, const char* filepath, char* it, u32 file_size)
{
	XMLElement root = xml_begin(it, file_size);

	if (root.corrupted) {
		SV_LOG_ERROR("Corrupted .dae\n");
		return FALSE;
	}

	XMLElement geometries = root;

	if (xml_enter_child(&geometries, "library_geometries")) {

	}
	else {
		SV_LOG_ERROR("Library geometries not found in .dae\n");
		return FALSE;
	}

	return TRUE;
}

b8 model_load(ModelInfo* model_info, const char* filepath)
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

	// Get folder path
	{
		const char* name = filepath_name(filepath);
		u32 folderpath_size = name - filepath;
		memory_copy(model_info->folderpath, filepath, folderpath_size);
		model_info->folderpath[folderpath_size] = '\0';
	}

	b8 res = TRUE;

	model_info->meshes = array_init(MeshInfo, 1.3f);
	model_info->materials = array_init(MaterialInfo, 1.3f);
	
	// Load file
	u8* file_data = NULL;
	u32 file_size = 0;
	if (file_read_binary(filepath, &file_data, &file_size)) {
		
		switch (model_format)
		{
		case 0:
			model_load_obj(model_info, filepath, file_data);
			break;
		case 1:
			model_load_dae(model_info, filepath, file_data, file_size);
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

	if (!res) {
		model_free(model_info);
	}

	return res;
}

void model_free(ModelInfo* model_info)
{
	// TODO
}
