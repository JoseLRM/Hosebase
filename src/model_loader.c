#include "Hosebase/serialize.h"

#include "assimp/cimport.h"     // Plain-C interface
#include "assimp/scene.h"       // Output data structure
#include "assimp/postprocess.h" // Post processing flags
#include "assimp/metadata.h"

inline v4 assimp_parse_quaternion(struct aiQuaternion v)
{
    return v4_set(v.x, v.y, v.z, v.w);
}

inline v3 assimp_parse_v3(struct aiVector3D v)
{
    return v3_set(v.x, v.y, v.z);
}

inline v2 assimp_parse_v2(struct aiVector2D v)
{
    return v2_set(v.x, v.y);
}

inline Color assimp_parse_color(struct aiColor4D c)
{
    return color_rgba_f32(c.r, c.g, c.b, c.a);
}

inline m4 assimp_parse_m4(struct aiMatrix4x4 m)
{
    m4 d;
    d.m00 = m.a1;
    d.m01 = m.a2;
    d.m02 = m.a3;
    d.m03 = m.a4;

    d.m10 = m.b1;
    d.m11 = m.b2;
    d.m12 = m.b3;
    d.m13 = m.b4;

    d.m20 = m.c1;
    d.m21 = m.c2;
    d.m22 = m.c3;
    d.m23 = m.c4;

    d.m30 = m.d1;
    d.m31 = m.d2;
    d.m32 = m.d3;
    d.m33 = m.d4;

    d = m4_transpose(d);

    return d;
}

static m4 assimp_compute_global_matrix(const struct aiScene *scene, const struct aiNode *node)
{
    m4 transform = m4_identity();

    if (node != NULL)
    {
        transform = assimp_parse_m4(node->mTransformation);

        while (node->mParent != NULL && node->mParent != scene->mRootNode)
        {
            node = node->mParent;
            m4 m = assimp_parse_m4(node->mTransformation);
            transform = m4_mul(transform, m);
        }
    }

    return transform;
}

static const struct aiNode *assimp_find_node_with_mesh(const struct aiNode *node, u32 mesh_index)
{
    foreach (i, node->mNumMeshes)
    {
        if (node->mMeshes[i] == mesh_index)
            return node;
    }

    foreach (i, node->mNumChildren)
    {
        const struct aiNode *n = node->mChildren[i];

        n = assimp_find_node_with_mesh(n, mesh_index);
        if (n != NULL)
            return n;
    }

    return NULL;
}

static u32 find_joint(ModelInfo *model_info, const char *name)
{
    foreach (i, model_info->joint_count)
    {
        JointInfo *joint = model_info->joints + i;

        if (string_equals(joint->name, name))
        {
            return i;
        }
    }

    return u32_max;
}

static u32 add_joint(ModelInfo *model_info, const struct aiScene *scene, const struct aiBone *bone)
{
    // Find
    {
        u32 index = find_joint(model_info, bone->mName.data);

        if (index != u32_max)
            return index;
    }

    if (model_info->joint_count >= SV_ARRAY_SIZE(model_info->joints))
    {
        SV_LOG_ERROR("The joint limit is %u\n", SV_ARRAY_SIZE(model_info->joints));
        return u32_max;
    }

    u32 parent_index = u32_max;

    // Find parent (assert)
    {
        const struct aiNode *parent = bone->mNode->mParent;

        if (parent != NULL && parent != bone->mArmature)
        {
            parent_index = find_joint(model_info, parent->mName.data);

            if (parent_index == u32_max)
            {
                SV_LOG_ERROR("Joint '%s' can't find his parent '%s'\n", bone->mName.data, parent->mName.data);
                return u32_max;
            }
        }
    }

    // Add
    {
        model_info->joint_count++;
        u32 joint_index;
        m4 parent_inverse_bind_matrix;

        if (parent_index == u32_max)
        {
            joint_index = model_info->joint_count - 1;
            parent_inverse_bind_matrix = m4_identity();
        }
        else
        {
            JointInfo *parent = model_info->joints + parent_index;

            parent_inverse_bind_matrix = parent->inverse_bind_matrix;

            joint_index = parent_index + parent->child_count + 1;

            for (i32 i = model_info->joint_count - 1; i >= joint_index; --i)
            {
                model_info->joints[i + 1] = model_info->joints[i];
            }

            while (parent != NULL)
            {
                parent->child_count++;
                parent = (parent->parent_index == u32_max) ? NULL : (model_info->joints + parent->parent_index);
            }
        }

        JointInfo *joint = model_info->joints + joint_index;
        string_copy(joint->name, bone->mName.data, NAME_SIZE);
        joint->parent_index = parent_index;

        joint->inverse_bind_matrix = assimp_parse_m4(bone->mOffsetMatrix);

        // Compute bind pose
        {
            m4 bind_matrix = m4_inverse(joint->inverse_bind_matrix);

            m4 local_matrix = m4_mul(bind_matrix, parent_inverse_bind_matrix);

            joint->local.position = m4_decompose_position(local_matrix);
            joint->local.rotation = v4_normalize(m4_decompose_rotation(local_matrix));
            joint->local.scale = m4_decompose_scale(local_matrix);
        }

        return joint_index;
    }
}

static void assimp_load_mesh(ModelInfo *model_info, MeshInfo *dst, const struct aiScene *scene, u32 mesh_index)
{
    const struct aiMesh *mesh = scene->mMeshes[mesh_index];
    u32 vertex_count = mesh->mNumVertices;
    u32 index_count = mesh->mNumFaces * 3;

    b8 has_animation_data = mesh->mNumBones > 0;

    // Allocate memory and asign pointers
    {
        u32 bytes = vertex_count * (sizeof(v3) + sizeof(v3) + sizeof(v2)) + index_count * sizeof(u32);

        if (has_animation_data)
            bytes += vertex_count * (sizeof(WeightInfo));

        dst->_memory = memory_allocate(bytes);
        dst->vertex_count = vertex_count;
        dst->index_count = index_count;

        u8 *it = dst->_memory;

        dst->positions = (v3 *)it;
        it += sizeof(v3) * vertex_count;

        dst->normals = (v3 *)it;
        it += sizeof(v3) * vertex_count;

        dst->texcoords = (v2 *)it;
        it += sizeof(v2) * vertex_count;

        if (has_animation_data)
        {
            dst->weights = (WeightInfo *)it;
            it += sizeof(WeightInfo) * vertex_count;
        }

        dst->indices = (u32 *)it;
    }

    m4 transform = m4_identity();
    m4 parent_transform = m4_identity();

    // Find transform matrix
    if (!has_animation_data)
    {
        const struct aiNode *node = assimp_find_node_with_mesh(scene->mRootNode, mesh_index);

        parent_transform = assimp_compute_global_matrix(scene, node->mParent);
        transform = assimp_parse_m4(node->mTransformation);

        transform = m4_mul(transform, parent_transform);
    }

    // Positions
    foreach (i, vertex_count)
    {
        v3 position = assimp_parse_v3(mesh->mVertices[i]);
        v4 p = v3_to_v4(position, 1.f);
        p = v4_transform(p, transform);
        position = v4_to_v3(p);
        dst->positions[i] = position;
    }

    // Normals
    {
        m4 m = transform;
        m.v[0][3] = 0.f;
        m.v[1][3] = 0.f;
        m.v[2][3] = 0.f;
        m.v[3][3] = 1.f;

        m = m4_inverse(m);
        m = m4_transpose(m);

        foreach (i, vertex_count)
        {
            v3 normal = assimp_parse_v3(mesh->mNormals[i]);
            v4 n = v3_to_v4(normal, 1.f);
            n = v4_transform(n, m);
            normal = v3_normalize(v4_to_v3(n));
            dst->normals[i] = normal;
        }
    }

    // Texcoord
    if (mesh->mTextureCoords[0] != NULL)
        foreach (i, vertex_count)
        {
            v2 v;
            v.x = mesh->mTextureCoords[0][i].x;
            v.y = mesh->mTextureCoords[0][i].y;
            dst->texcoords[i] = v;
        }

    // Indices
    foreach (i, mesh->mNumFaces)
    {
        const struct aiFace *face = mesh->mFaces + i;
        if (face->mNumIndices == 3)
        {
            u32 o = i * 3;
            dst->indices[o + 0] = face->mIndices[0];
            dst->indices[o + 1] = face->mIndices[1];
            dst->indices[o + 2] = face->mIndices[2];
        }
    }

    // Joints
    foreach (i, mesh->mNumBones)
    {
        const struct aiBone *bone = mesh->mBones[i];
        add_joint(model_info, scene, bone);
    }

    // Material
    {
        dst->material_index = mesh->mMaterialIndex;
    }
}

static void assimp_load_weights(ModelInfo *model_info, MeshInfo *dst, const struct aiScene *scene, u32 mesh_index)
{
    const struct aiMesh *mesh = scene->mMeshes[mesh_index];

    foreach (i, mesh->mNumBones)
    {
        const struct aiBone *bone = mesh->mBones[i];

        u32 joint_index = find_joint(model_info, bone->mName.data);

        assert(joint_index != u32_max);

        foreach (w, bone->mNumWeights)
        {
            const struct aiVertexWeight *weight = bone->mWeights + w;

            WeightInfo *weight_dst = dst->weights + weight->mVertexId;

            if (weight_dst->count < SV_ARRAY_SIZE(weight_dst->joint_indices))
            {
                weight_dst->weights[weight_dst->count] = (f32)weight->mWeight;
                weight_dst->joint_indices[weight_dst->count] = joint_index;

                weight_dst->count++;
            }
            else
            {
                u32 lower_joint = 0;

                for (u32 j = 1; j < weight_dst->count; ++j)
                {
                    if (weight_dst->weights[j] < weight_dst->weights[lower_joint])
                    {
                        lower_joint = j;
                    }
                }

                if (weight->mWeight > weight_dst->weights[lower_joint])
                {
                    weight_dst->weights[lower_joint] = (f32)weight->mWeight;
                    weight_dst->joint_indices[lower_joint] = joint_index;
                }
            }
        }
    }

    if (mesh->mNumBones > 0)
        foreach (i, dst->vertex_count)
        {
            WeightInfo *w = dst->weights + i;

            f32 total = 0.f;

            foreach (j, w->count)
            {
                total += w->weights[j];
            }

            foreach (j, w->count)
            {
                w->weights[j] /= total;
            }
        }
}

static void assimp_load_material(MaterialInfo *dst, const struct aiMaterial *material, const char *folder)
{
    // Colors
    {
        struct aiColor4D ambient_color = {.r = 0.f, .g = 0.f, .b = 0.f, .a = 1.f};
        struct aiColor4D diffuse_color = {.r = 1.f, .g = 1.f, .b = 1.f, .a = 1.f};
        struct aiColor4D emissive_color = {.r = 0.f, .g = 0.f, .b = 0.f, .a = 1.f};
        struct aiColor4D specular_color = {.r = 0.f, .g = 0.f, .b = 0.f, .a = 0.f};
        aiGetMaterialColor(material, AI_MATKEY_COLOR_AMBIENT, &ambient_color);
        aiGetMaterialColor(material, AI_MATKEY_COLOR_DIFFUSE, &diffuse_color);
        aiGetMaterialColor(material, AI_MATKEY_COLOR_EMISSIVE, &emissive_color);
        aiGetMaterialColor(material, AI_MATKEY_COLOR_SPECULAR, &specular_color);
        dst->ambient_color = assimp_parse_color(ambient_color);
        dst->diffuse_color = assimp_parse_color(diffuse_color);
        dst->emissive_color = assimp_parse_color(emissive_color);
        dst->specular_color = assimp_parse_color(specular_color);
    }

    // Misc
    {
        int twoSided = 0;
        aiGetMaterialInteger(material, AI_MATKEY_TWOSIDED, &twoSided);
        dst->culling = twoSided ? CullMode_None : CullMode_Back;

        dst->shininess = 0.f;
        aiGetMaterialFloat(material, AI_MATKEY_SHININESS, &dst->shininess);

        int transparent;
        aiGetMaterialInteger(material, AI_MATKEY_BLEND_FUNC, &transparent);
        dst->transparent = transparent;
    }

    // Name
    {
        struct aiString name;
        aiGetMaterialString(material, AI_MATKEY_NAME, &name);
        string_copy(dst->name, name.data, NAME_SIZE);
    }

    // Textures
    {
        foreach (i, AI_TEXTURE_TYPE_MAX)
        {
            if (aiGetMaterialTextureCount(material, i) != 0)
            {
                struct aiString path;
                aiGetMaterialTexture(material, i, 0, &path, NULL, NULL, NULL, NULL, NULL, NULL);

                string_copy(dst->diffuse_map, path.data, FILE_PATH_SIZE);
                SV_LOG_INFO("%u\n", i);
                break;
            }
        }

        // TODO:
        /*
        dst->emissive_map;
        dst->normal_map;
        dst->specular_map;

        if (material->GetTextureCount(aiTextureType_NORMALS) != 0)
        {
            aiString path0;
            material->GetTexture(aiTextureType_NORMALS, 0, &path0);
            jsh::Texture *normalMap = jshGraphics::CreateTexture((std::string(meshName) + "[normal]").c_str());
            jshLoader::LoadTexture((std::string(path) + std::string(path0.C_Str())).c_str(), &normalMap->resource);
            normalMap->samplerState = &jshGraphics::primitives::GetDefaultSamplerState();
            shader->SetNormalMap(normalMap, matData);
        }
        if (material->GetTextureCount(aiTextureType_SPECULAR) != 0)
        {
            aiString path0;
            material->GetTexture(aiTextureType_SPECULAR, 0, &path0);
            jsh::Texture *specularMap = jshGraphics::CreateTexture((std::string(meshName) + "[specular]").c_str());
            jshLoader::LoadTexture((std::string(path) + std::string(path0.C_Str())).c_str(), &specularMap->resource);
            specularMap->samplerState = &jshGraphics::primitives::GetDefaultSamplerState();
            shader->SetSpecularMap(specularMap, matData);
        }*/
    }
}

static void assimp_load_animation(ModelInfo *model_info, const struct aiScene *scene, AnimationInfo *dst, const struct aiAnimation *animation)
{
    string_copy(dst->name, animation->mName.data, NAME_SIZE);

    dst->total_keyframe_count = 0;
    dst->total_time = animation->mDuration / animation->mTicksPerSecond;

    // Count total keyframes and validate
    {
        b8 valid_animation = TRUE;

        foreach (i, animation->mNumChannels)
        {
            const struct aiNodeAnim *channel = animation->mChannels[i];
            if_assert(channel->mNumPositionKeys == channel->mNumRotationKeys && channel->mNumRotationKeys == channel->mNumScalingKeys)
            {
                dst->total_keyframe_count += channel->mNumPositionKeys;
            }
            else
            {
                valid_animation = FALSE;
                break;
            }
        }

        if (!valid_animation)
        {
            SV_LOG_ERROR("The animation '%s' is not valid\n", animation->mName.data);
            return;
        }
        if (dst->total_keyframe_count == 0)
        {
            SV_LOG_ERROR("The animation '%s' has 0 keyframes\n", animation->mName.data);
            return;
        }
    }

    // Allocate memory
    {
        dst->_keyframe_memory = memory_allocate(dst->total_keyframe_count * sizeof(KeyFrameInfo));
    }

    // Save keyframes
    {
        KeyFrameInfo *it = dst->_keyframe_memory;

        foreach (channel_index, animation->mNumChannels)
        {
            const struct aiNodeAnim *channel = animation->mChannels[channel_index];

            assert(channel->mNumPositionKeys == channel->mNumRotationKeys && channel->mNumRotationKeys == channel->mNumScalingKeys);

            u32 joint_index = u32_max;

            // Find joint
            {
                foreach (j, model_info->joint_count)
                {
                    JointInfo *joint = model_info->joints + j;
                    if (string_equals(joint->name, channel->mNodeName.data))
                    {
                        joint_index = j;
                        break;
                    }
                }
            }

            if (joint_index == u32_max)
            {
                SV_LOG_ERROR("Joint '%s' not found\n", channel->mNodeName.data);
                continue;
            }

            JointAnimationInfo *info = dst->joint_animations + joint_index;
            JointInfo* joint = model_info->joints + joint_index;

            info->keyframe_count = channel->mNumPositionKeys;
            info->keyframes = it;
            it += info->keyframe_count;

            b8 empty_keyframes = TRUE;

            foreach (j, channel->mNumPositionKeys)
            {
                m4 matrix;
                {
                    struct aiMatrix4x4 mat;
                    struct aiVector3D position = channel->mPositionKeys[j].mValue;
                    struct aiQuaternion rotation = channel->mRotationKeys[j].mValue;
                    struct aiVector3D scale = channel->mScalingKeys[j].mValue;

                    aiMatrix4FromScalingQuaternionPosition(&mat, &scale, &rotation, &position);
                    matrix = assimp_parse_m4(mat);

                    if (joint->parent_index == u32_max)
                    {
                        m4 root = assimp_parse_m4(scene->mRootNode->mTransformation);
                        root = m4_inverse(root);
                        matrix = m4_mul(matrix, root);
                    }
                }

                f32 time = (f32)(channel->mPositionKeys[j].mTime / animation->mTicksPerSecond);

                v3 position = m4_decompose_position(matrix);
                v4 rotation = v4_normalize(m4_decompose_rotation(matrix));
                v3 scale = m4_decompose_scale(matrix);

                if (!v3_equals(position, joint->local.position) || !v4_equals(rotation, joint->local.rotation) || !v3_equals(scale, joint->local.scale))
                {
                    empty_keyframes = FALSE;
                }

                info->keyframes[j].position = position;
                info->keyframes[j].rotation = rotation;
                info->keyframes[j].scale = scale;
                info->keyframes[j].time = time;
            }

            if (empty_keyframes)
            {
                info->keyframe_count = 0;
                info->keyframes = NULL;
            }
        }
    }
}

static void print_tabs(u32 tabs)
{
    foreach (i, tabs)
        SV_LOG_INFO("   ");
}

static void print_debug_joint_data(ModelInfo *model_info, u32 joint_index, u32 tabs)
{
    JointInfo *joint = model_info->joints + joint_index;

    print_tabs(tabs);
    SV_LOG_INFO("%s: %u childs\n", joint->name, joint->child_count);

    print_tabs(tabs);
    m4 m = joint->inverse_bind_matrix;
    SV_LOG_INFO("   Inverse Bind Matrix:\n");
    print_tabs(tabs);
    SV_LOG_INFO("   %f, %f, %f, %f\n", m.m00, m.m01, m.m02, m.m03);
    print_tabs(tabs);
    SV_LOG_INFO("   %f, %f, %f, %f\n", m.m10, m.m11, m.m12, m.m13);
    print_tabs(tabs);
    SV_LOG_INFO("   %f, %f, %f, %f\n", m.m20, m.m21, m.m22, m.m23);
    print_tabs(tabs);
    SV_LOG_INFO("   %f, %f, %f, %f\n", m.m30, m.m31, m.m32, m.m33);

    foreach (i, joint->child_count)
    {
        u32 child_index = joint_index + i + 1;
        JointInfo *child = model_info->joints + child_index;

        print_debug_joint_data(model_info, child_index, tabs + 1);
        i += child->child_count;
    }
}

static void assimp_load_scene(ModelInfo *model_info, const struct aiScene *scene, const char *filepath)
{
    char folder[FILE_PATH_SIZE];
    {
        u32 folder_size = filepath_folder(filepath);
        string_set(folder, filepath, folder_size, FILE_PATH_SIZE);
    }

    // MESHES
    {
        model_info->mesh_count = scene->mNumMeshes;

        // Read common vertex data and joint hierarchy
        foreach (i, model_info->mesh_count)
        {
            const struct aiMesh *mesh = scene->mMeshes[i];
            MeshInfo *dst = model_info->meshes + i;

            string_copy(dst->name, mesh->mName.data, NAME_SIZE);
            assimp_load_mesh(model_info, dst, scene, i);
        }

        // Add joint indices in each vertex
        foreach (i, model_info->mesh_count)
        {
            const struct aiMesh *mesh = scene->mMeshes[i];
            MeshInfo *dst = model_info->meshes + i;

            assimp_load_weights(model_info, dst, scene, i);
        }
    }

    // MATERIALS
    {
        model_info->material_count = scene->mNumMaterials;

        foreach (i, model_info->material_count)
        {
            const struct aiMaterial *material = scene->mMaterials[i];
            MaterialInfo *dst = model_info->materials + i;

            string_copy(dst->name, "unnamed", NAME_SIZE);
            assimp_load_material(dst, material, folder);
        }
    }

    // ANIMATIONS
    {
        model_info->animation_count = scene->mNumAnimations;

        foreach (i, model_info->animation_count)
        {
            const struct aiAnimation *animation = scene->mAnimations[i];
            AnimationInfo *dst = model_info->animations + i;

            assimp_load_animation(model_info, scene, dst, animation);
        }
    }

    // DEBUG
    if (0)
    {
        if (0)
        {
            // Joints
            foreach (i, model_info->joint_count)
            {
                JointInfo *j = model_info->joints + i;

                print_debug_joint_data(model_info, i, 0);

                i += j->child_count;
            }
        }

        // Animations
        foreach (i, model_info->animation_count)
        {
            AnimationInfo *a = model_info->animations + i;

            SV_LOG_INFO("Animation '%s', %f seconds:\n", a->name, a->total_time);

            foreach (w, model_info->joint_count)
            {
                JointAnimationInfo *ja = a->joint_animations + w;
                JointInfo *j = model_info->joints + w;

                SV_LOG_INFO("   %s:\n", j->name);

                foreach (k, ja->keyframe_count)
                {
                    KeyFrameInfo *kf = ja->keyframes + k;
                    SV_LOG_INFO("       %f, (%f.2, %f.2, %f.2), (%f.2, %f.2, %f.2, %f.2), (%f.2, %f.2, %f.2)\n", kf->time, kf->position.x, kf->position.y, kf->position.z, kf->rotation.x, kf->rotation.y, kf->rotation.z, kf->rotation.w, kf->scale.x, kf->scale.y, kf->scale.z);
                }
            }
        }
    }
}

b8 import_model(ModelInfo *model_info, const char *filepath)
{
    memory_zero(model_info, sizeof(ModelInfo));

    aiSetImportPropertyFloat(aiCreatePropertyStore(), AI_CONFIG_GLOBAL_SCALE_FACTOR_KEY, 1.0f);

    // Start the import on the given file with some example postprocessing
    // Usually - if speed is not the most important aspect for you - you'll t
    // probably to request more postprocessing than we do in this example.
    const struct aiScene *scene = aiImportFile(filepath,
                                               aiProcess_CalcTangentSpace |
                                                   aiProcess_ConvertToLeftHanded |
                                                   aiProcess_Triangulate |
                                                   aiProcess_GlobalScale |
                                                   aiProcess_PopulateArmatureData);

    // If the import failed, report it
    if (scene == NULL)
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
    assimp_load_scene(model_info, scene, filepath);

    aiReleaseImport(scene);
    return TRUE;

error_exit:
    aiReleaseImport(scene);
    return FALSE;
}