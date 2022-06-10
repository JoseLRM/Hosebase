#include "serialize.h"

#include "assimp/cimport.h"     // Plain-C interface
#include "assimp/scene.h"       // Output data structure
#include "assimp/postprocess.h" // Post processing flags

static void assimp_load_mesh(MeshInfo* dst, const struct aiMesh* mesh)
{
    
}

static void assimp_load_scene(ModelInfo *model_info, const struct aiScene *scene)
{
    // MESHES
	u32 mesh_count = scene->mNumMeshes;

	foreach(i, mesh_count) 
    {
	    const struct aiMesh* mesh = scene->mMeshes[i];
        MeshInfo* dst = model_info->meshes + i;
        
        string_copy(dst->name, mesh->mName.data, NAME_SIZE);
		assimp_load_mesh(dst, mesh);
	}

    /*
	// NODES
	aiNode* aiRoot = scene->mRootNode;
	if (aiRoot) {
		AddNode(aiRoot, &model->root, meshes);
	}
    */
}

b8 import_model2(ModelInfo *model_info, const char *filepath)
{
    // Start the import on the given file with some example postprocessing
    // Usually - if speed is not the most important aspect for you - you'll t
    // probably to request more postprocessing than we do in this example.
    const struct aiScene *scene = aiImportFile(filepath,
                                               aiProcess_CalcTangentSpace |
                                                   aiProcess_Triangulate |
                                                   aiProcess_JoinIdenticalVertices |
                                                   aiProcess_SortByPType);

    // If the import failed, report it
    if (NULL != scene)
    {
        SV_LOG_ERROR("Can't import 3D model: '%s'\n", aiGetErrorString());
        return FALSE;
    }

    if (scene->mNumMeshes == 0)
    {
        SV_LOG_ERROR("Can't import empty 3D model '%s'\n", filepath);
        return FALSE;
    }

    // Check limits
    {
        if (SV_ARRAY_SIZE(model_info->meshes) < scene->mNumMeshes)
        {
            SV_LOG_ERROR("Can't import 3D model '%s', mesh limit of %u exceeded\n", filepath, scene->mNumMeshes);
            goto error_exit;
        }
    }

    // Now we can access the file's contents
    assimp_load_scene(model_info, scene);

    aiReleaseImport(scene);
    return TRUE;

error_exit:
    aiReleaseImport(scene);
    return FALSE;
}