#pragma once

#if SV_MODEL_LOADER

#include "Hosebase/graphics.h"

#define MODEL_INFO_MAX_MESHES 50
#define MODEL_INFO_MAX_JOINTS 100
#define MODEL_INFO_MAX_MATERIALS 50
#define MODEL_INFO_MAX_WEIGHTS 7
#define MODEL_INFO_MAX_ARMATURES 10
#define MODEL_INFO_MAX_KEYFRAMES 200
#define MODEL_INFO_MAX_ANIMATIONS 50

typedef struct {
	u32 joint_indices[MODEL_INFO_MAX_WEIGHTS];
	f32 weights[MODEL_INFO_MAX_WEIGHTS];
	u32 count;
} WeightInfo;

typedef struct {

	char name[NAME_SIZE];

	u8* _memory;

	v3* positions;
	v3* normals;
	v2* texcoords;
	WeightInfo* weights;
	u32 vertex_count;

	u32* indices;
	u32 index_count;

	u32 material_index;
	m4 skin_bind_matrix;

} MeshInfo;

typedef struct {
	char name[NAME_SIZE];
	// TODO: Remove unnecesary matrices
	struct {
		v3 position;
		v4 rotation;
		v3 scale;
	} local;
	m4 inverse_bind_matrix;
	u8 child_count;
	u32 parent_index;
} JointInfo;

typedef struct {
	v3 position;
	v4 rotation;
	v3 scale;
	f32 time;
} KeyFrameInfo;

typedef struct {
	KeyFrameInfo* keyframes;
	u32 keyframe_count;
} JointAnimationInfo;

typedef struct {
	KeyFrameInfo* _keyframe_memory;
	u32 total_keyframe_count;
	f32 total_time;

	char name[NAME_SIZE];
	JointAnimationInfo joint_animations[MODEL_INFO_MAX_JOINTS];
} AnimationInfo;

typedef struct {

	char name[NAME_SIZE];
	
	// Pipeline settings
	b8 transparent;
	CullMode culling;

	// Values
	
	Color ambient_color;
	Color diffuse_color;
	Color specular_color;
	Color emissive_color;
	f32 shininess;

	// Textures
	
	char diffuse_map[FILE_PATH_SIZE];
	char normal_map[FILE_PATH_SIZE];
	char specular_map[FILE_PATH_SIZE];
	char emissive_map[FILE_PATH_SIZE];

} MaterialInfo;

typedef struct {
	char folderpath[FILE_PATH_SIZE];

	MeshInfo meshes[MODEL_INFO_MAX_MESHES];
	u32 mesh_count;

	MaterialInfo materials[MODEL_INFO_MAX_MATERIALS];
	u32 material_count;

	AnimationInfo animations[MODEL_INFO_MAX_ANIMATIONS];
	u32 animation_count;
	m4 animation_matrix; // Transform the roots by this matrix to be in the right coordinate system

	JointInfo joints[MODEL_INFO_MAX_JOINTS];
	u32 joint_count;
	
} ModelInfo;

b8 import_model(ModelInfo* model_info, const char* filepath);
void free_model_info(ModelInfo* model_info);

#endif