#include "ModelLoader.h"

#define CGLTF_IMPLEMENTATION
#pragma warning(disable:4996)
#include <gltf/cgltf.h>
//#include "Texture.h"
//#include "Material.h"
#include "Utils/GenericUtils.h"
#include "Utils/TimeUtils.h"
#include "Utils/FileSystem.h"
//#include "VulkanRenderEngine.h"
#include "glm/ext/quaternion_float.hpp"
#include "glm/gtx/quaternion.hpp"
#include "Core/Logger.h"
#include "Core/Debug.h"
#undef CGLTF_IMPLEMENTATION


#include "RenderSystem.h"

#define GLTF_LOAD_GEOMETRY_POSITION 0x01
#define GLTF_LOAD_GEOMETRY_NORMAL 0x02
#define GLTF_LOAD_GEOMETRY_COLOR 0x04
#define GLTF_LOAD_GEOMETRY_TANGENT 0x08
#define GLTF_LOAD_GEOMETRY_TEXCOORDS 0x10
#define GLTF_LOAD_GEOMETRY_JOINTS 0x20
#define GLTF_LOAD_GEOMETRY_WEIGHTS 0x40
#define GLTF_LOAD_GEOMETRY_ALL 0xff

//#define MESH_DUMP_LOAD_INFO
#ifdef MESH_DUMP_LOAD_INFO
#define loadmeshlabel "[loadmesh] "
#define loadmeshlogf(fmt, ...) logfinfo(loadmeshlabel fmt, __VA_ARGS__)
#define loadmeshlog(fmt, ...) loginfo(loadmeshlabel fmt)
#else
#define loadmeshlogf(fmt, ...) DUMMY_MACRO
#define loadmeshlog(fmt, ...) DUMMY_MACRO
#endif

#define LOAD_MESH_CHECK_READ_ACCESSOR
#ifdef LOAD_MESH_CHECK_READ_ACCESSOR
#define check_accessor(_condition) check((_condition))
#define check_accessor_call(_f) check((_f))
#else
#define check_accessor(_f_) DUMMY_MACRO
#define check_accessor_call(_f) expand(_f)
#endif


namespace gltf_api
{
    void HandleError(cgltf_result result, const char* filepath)
    {
        switch (result)
        {
        case cgltf_result_success:
            break;
        case cgltf_result_data_too_short:
            break;
        case cgltf_result_unknown_format:
            break;
        case cgltf_result_invalid_json:
            break;
        case cgltf_result_invalid_gltf:
            break;
        case cgltf_result_invalid_options:
            break;
        case cgltf_result_file_not_found:
            break;
        case cgltf_result_io_error:
            break;
        case cgltf_result_out_of_memory:
            break;
        case cgltf_result_legacy_gltf:
            break;
        case cgltf_result_max_enum:
            break;
        default:
            break;
        }
    }

    float Length2(const glm::vec3& vec)
    {
        return vec.x * vec.x + vec.y * vec.y + vec.z * vec.z;
    }

    float Length(const glm::vec3& vec) { return sqrtf(Length2(vec)); }

    uint32_t GetElementCountFromType(cgltf_type type)
    {
        switch (type)
        {
        case cgltf_type_scalar: return 1;
        case cgltf_type_vec2: return 2;
        case cgltf_type_vec3: return 3;
        case cgltf_type_vec4: return 4;
        case cgltf_type_mat2: return 2 * 2;
        case cgltf_type_mat3: return 3 * 3;
        case cgltf_type_mat4: return 4 * 4;
        case cgltf_type_invalid:
        case cgltf_type_max_enum:
        default:
            check(false && "Invalid cgltf_type.");
            break;
        }
        return 0;
    }

    void ReadValue(void* dst, const cgltf_float* data, uint32_t count)
    {
        memcpy_s(dst, sizeof(float) * count, data, sizeof(float) * count);
    }

    void ToMat4(glm::mat4* mat, const cgltf_float* cgltfMat4)
    {
        ReadValue(mat, cgltfMat4, 16);
    }

    void ToVec2(glm::vec2& v, const cgltf_float* data)
    {
        ReadValue(&v, data, 2);
    }

    void ToVec3(glm::vec3& v, const cgltf_float* data)
    {
        ReadValue(&v, data, 3);
    }

    void ToVec4(glm::vec4& v, const cgltf_float* data)
    {
        ReadValue(&v, data, 4);
    }

    void ToQuat(glm::quat& q, const cgltf_float* data)
    {
        q = glm::quat(data[3], data[0], data[1], data[2]);
    }

    void ReadNodeLocalTransform(const cgltf_node& node, glm::mat4& t)
    {
        t = glm::mat4(1.f);
        if (node.has_matrix)
        {
            gltf_api::ToMat4(&t, node.matrix);
        }
        else
        {
            if (node.has_translation)
            {
                glm::vec3 pos;
                gltf_api::ToVec3(pos, node.translation);
                t *= glm::translate(t, pos);
            }
            if (node.has_rotation)
            {
                glm::quat quat;
                gltf_api::ToQuat(quat, node.rotation);
                t *= glm::toMat4(quat);
            }
            if (node.has_scale)
            {
                glm::vec3 scl;
                gltf_api::ToVec3(scl, node.scale);
                t = glm::scale(t, scl);
            }
        }
    }

#if 0
    // Attributes are an continuous array of positions, normals, uvs...
    void ReadAttributeArray(Mist::Vertex* vertices, const cgltf_attribute& attribute)
    {
        const cgltf_accessor* accessor = attribute.data;
        check(accessor->count < UINT32_MAX);
        uint32_t count = (uint32_t)accessor->count;
        // Get how many values has current attribute
        cgltf_size numComponents = cgltf_num_components(accessor->type);
        check_accessor(accessor->component_type == cgltf_component_type_r_32f);
        switch (attribute.type)
        {
        case cgltf_attribute_type_position:
        {
            check_accessor(accessor->has_min && accessor->has_max);
            check_accessor(accessor->type == cgltf_type_vec3 && numComponents == 3);
            check_accessor(!strcmp(attribute.name, "POSITION"));

            for (uint32_t i = 0; i < count; ++i)
                check_accessor_call(cgltf_accessor_read_float(accessor, i, &vertices[i].Position[0], numComponents));

            break;
        }
        case cgltf_attribute_type_texcoord:
            check_accessor(accessor->type == cgltf_type_vec2 && numComponents == 2);
            if (!strcmp(attribute.name, "TEXCOORD_0"))
            {
                for (uint32_t i = 0; i < count; ++i)
                    check_accessor_call(cgltf_accessor_read_float(accessor, i, &vertices[i].TexCoords0[0], numComponents));
            }
            else if (!strcmp(attribute.name, "TEXCOORD_1"))
            {
                for (uint32_t i = 0; i < count; ++i)
                    check_accessor_call(cgltf_accessor_read_float(accessor, i, &vertices[i].TexCoords1[0], numComponents));
            }
            else
                unreachable_code();
            break;
        case cgltf_attribute_type_normal:
            check_accessor(accessor->type == cgltf_type_vec3 && numComponents == 3);
            check_accessor(!strcmp(attribute.name, "NORMAL"));
            for (uint32_t i = 0; i < count; ++i)
            {
                check_accessor_call(cgltf_accessor_read_float(accessor, i, &vertices[i].Normal[0], numComponents));
                check_accessor(Length2(vertices[i].Normal) > 1e-5f);
            }
            break;
        case cgltf_attribute_type_color:
            check_accessor(accessor->type == cgltf_type_vec3 && numComponents == 3);
            check_accessor(!strcmp(attribute.name, "COLOR_0"));
            for (uint32_t i = 0; i < count; ++i)
            {
                check_accessor_call(cgltf_accessor_read_float(accessor, i, &vertices[i].Color[0], numComponents));
                check_accessor(vertices[i].Color.x >= 0.f && vertices[i].Color.x <= 1.f);
                check_accessor(vertices[i].Color.y >= 0.f && vertices[i].Color.y <= 1.f);
                check_accessor(vertices[i].Color.z >= 0.f && vertices[i].Color.z <= 1.f);
            }
            break;
        case cgltf_attribute_type_tangent:
            check_accessor(accessor->type == cgltf_type_vec4 && numComponents == 4);
            check_accessor(!strcmp(attribute.name, "TANGENT"));
            for (uint32_t i = 0; i < count; ++i)
            {
                check_accessor_call(cgltf_accessor_read_float(accessor, i, &vertices[i].Tangent[0], numComponents));
                check_accessor(vertices[i].Tangent.w == -1.f || vertices[i].Tangent.w == 1.f);
            }
            break;
        case cgltf_attribute_type_invalid:
        case cgltf_attribute_type_custom:
        case cgltf_attribute_type_max_enum:
        default:
            check(false && "Invalid attribute type to read.");
            break;
        }
    }
#else

    float* GetAttributeInBuffer(float* buffer, uint64_t bufferSizeInBytes, uint32_t elementIndex, uint32_t offset, uint32_t stride, uint32_t attributeNumComponents)
    {
        float* p = buffer + offset + stride * elementIndex;
        check_accessor((uint64_t(p + attributeNumComponents - 1) - uint64_t(buffer)) < bufferSizeInBytes);
        return p;
    }

    float* ReadAttribute(float* buffer, uint64_t bufferSizeInBytes, uint32_t elementIndex, uint32_t offset, uint32_t stride, const cgltf_accessor* accessor)
    {
        uint32_t nc = (uint32_t)cgltf_num_components(accessor->type);
        float* p = GetAttributeInBuffer(buffer, bufferSizeInBytes, elementIndex, offset, stride, nc);
        check_accessor_call(cgltf_accessor_read_float(accessor, elementIndex, p, nc));
        return p;
    }

    // Attributes are an continuous array of positions, normals, uvs...
    void ReadAttributeArray(float* buffer, uint64_t bufferSizeInBytes, uint32_t offset, uint32_t stride, const cgltf_attribute& attribute)
    {
        const cgltf_accessor* accessor = attribute.data;
        uint32_t count = Mist::limits_cast<uint32_t>(accessor->count);

        // Get how many values has current attribute
        cgltf_size numComponents = cgltf_num_components(accessor->type);
        check_accessor(accessor->component_type == cgltf_component_type_r_32f);
        switch (attribute.type)
        {
        case cgltf_attribute_type_position:
        {
            check_accessor(accessor->has_min && accessor->has_max);
            check_accessor(accessor->type == cgltf_type_vec3 && numComponents == 3);
            check_accessor(!strcmp(attribute.name, "POSITION"));
            for (uint32_t i = 0; i < count; ++i)
                ReadAttribute(buffer, bufferSizeInBytes, i, offset, stride, accessor);
            break;
        }
        case cgltf_attribute_type_texcoord:
            check_accessor(accessor->type == cgltf_type_vec2 && numComponents == 2);
            check_accessor(!strcmp(attribute.name, "TEXCOORD_0") || !strcmp(attribute.name, "TEXCOORD_1"));
            for (uint32_t i = 0; i < count; ++i)
                ReadAttribute(buffer, bufferSizeInBytes, i, offset, stride, accessor);
            break;
        case cgltf_attribute_type_normal:
            check_accessor(accessor->type == cgltf_type_vec3 && numComponents == 3);
            check_accessor(!strcmp(attribute.name, "NORMAL"));
            for (uint32_t i = 0; i < count; ++i)
            {
                float* p = ReadAttribute(buffer, bufferSizeInBytes, i, offset, stride, accessor);
                check_accessor(Length2({p[0], p[1], p[2]}) > 1e-5f);
            }
            break;
        case cgltf_attribute_type_color:
            check_accessor(accessor->type == cgltf_type_vec3 && numComponents == 3);
            check_accessor(!strcmp(attribute.name, "COLOR_0"));
            for (uint32_t i = 0; i < count; ++i)
            {
                float* p = ReadAttribute(buffer, bufferSizeInBytes, i, offset, stride, accessor);
                check_accessor(p[0] >= 0.f && p[0] <= 1.f);
                check_accessor(p[1] >= 0.f && p[1] <= 1.f);
                check_accessor(p[2] >= 0.f && p[2] <= 1.f);
            }
            break;
        case cgltf_attribute_type_tangent:
            check_accessor(accessor->type == cgltf_type_vec4 && numComponents == 4);
            check_accessor(!strcmp(attribute.name, "TANGENT"));
            for (uint32_t i = 0; i < count; ++i)
            {
                float* p = ReadAttribute(buffer, bufferSizeInBytes, i, offset, stride, accessor);
                check_accessor(p[3] == -1.f || p[3] == 1.f);
            }
            break;
        case cgltf_attribute_type_invalid:
        case cgltf_attribute_type_custom:
        case cgltf_attribute_type_max_enum:
        default:
            check(false && "Invalid attribute type to read.");
            break;
        }
    }
#endif // 0


    void FreeData(cgltf_data* data)
    {
        cgltf_free(data);
    }

    cgltf_data* ParseFile(const char* filepath)
    {
        cgltf_options options;
        memset(&options, 0, sizeof(cgltf_options));
        cgltf_data* data{ nullptr };
        cgltf_result result = cgltf_parse_file(&options, filepath, &data);
        if (result != cgltf_result_success)
        {
            HandleError(result, filepath);
            return nullptr;
        }
        result = cgltf_load_buffers(&options, data, filepath);
        if (result != cgltf_result_success)
        {
            HandleError(result, filepath);
            return nullptr;
        }
        result = cgltf_validate(data);
        if (result != cgltf_result_success)
        {
            HandleError(result, filepath);
            FreeData(data);
            return nullptr;
        }
        return data;
    }

#if 0
    void LoadVertices(const cgltf_primitive& primitive, Mist::Vertex* verticesOut, uint32_t vertexCount)
    {
        uint32_t attributeCount = (uint32_t)primitive.attributes_count;
        check(primitive.attributes[0].data->count < UINT32_MAX);
        uint32_t accessorCount = (uint32_t)primitive.attributes[0].data->count;
        for (uint32_t i = 0; i < attributeCount; ++i)
        {
            const cgltf_attribute& attribute = primitive.attributes[i];
            check(attribute.data->count <= vertexCount);
            check(attribute.data->count == accessorCount);
            ReadAttributeArray(verticesOut, attribute);
        }
    }
#else
    rendersystem::modelloader::AttributeType GetAttributeType(const cgltf_attribute& attribute)
    {
        switch (attribute.type)
        {
        case cgltf_attribute_type_position: return rendersystem::modelloader::Attribute_Position;
        case cgltf_attribute_type_texcoord:
            if (!strcmp(attribute.name, "TEXCOORD0")) return rendersystem::modelloader::Attribute_TexCoord0;
            if (!strcmp(attribute.name, "TEXCOORD1")) return rendersystem::modelloader::Attribute_TexCoord1;
            unreachable_code();
        case cgltf_attribute_type_normal: return rendersystem::modelloader::Attribute_Normal;
        case cgltf_attribute_type_color: return rendersystem::modelloader::Attribute_VertexColor;
        case cgltf_attribute_type_tangent: return rendersystem::modelloader::Attribute_Tangent;
        case cgltf_attribute_type_invalid:
        case cgltf_attribute_type_custom:
        case cgltf_attribute_type_max_enum:
        default:
            unreachable_code();
        }
        return rendersystem::modelloader::Attribute_Count;
    }

    void LoadVertices(const cgltf_primitive& primitive, float* buffer, uint32_t bufferSizeInBytes, const rendersystem::modelloader::VertexLayout& layout)
    {
        uint32_t attributeCount = Mist::limits_cast<uint32_t>(primitive.attributes_count);
        for (uint32_t i = 0; i < attributeCount; ++i)
        {
            const cgltf_attribute& attribute = primitive.attributes[i];
            rendersystem::modelloader::AttributeType type = GetAttributeType(attribute);
            check(layout.attributeMask & (1 << type));
            check(bufferSizeInBytes % layout.size == 0);
            uint32_t vertexCount = bufferSizeInBytes / layout.size;
            check(attribute.data->count <= vertexCount);
            check(attribute.data->count == primitive.attributes[0].data->count);
            ReadAttributeArray(buffer, bufferSizeInBytes, layout.offsets[type], layout.size, attribute);
        }
    }
#endif // 0


    void LoadIndices(const cgltf_primitive& primitive, uint32_t* indicesOut, uint32_t offset)
    {
        check(primitive.indices && primitive.type == cgltf_primitive_type_triangles);
        check(primitive.indices->count < UINT32_MAX);
        uint32_t indexCount = (uint32_t)primitive.indices->count;
        for (uint32_t i = 0; i < indexCount; ++i)
        {
            size_t index = cgltf_accessor_read_index(primitive.indices, i);
            check(index + (size_t)offset < UINT32_MAX);
            indicesOut[i] = (uint32_t)index + offset;
        }
    }

#if 0
    bool LoadTexture(const Mist::RenderContext& context, const char* rootAssetPath, const cgltf_texture_view& texView, Mist::EFormat format, Mist::cTexture** texOut)
    {
        if (!texView.texture)
            return false;
        check(texView.texcoord == 0);
        check(texView.scale == 1.f && !texView.has_transform);
        char texturePath[512];
        sprintf_s(texturePath, "%s%s", rootAssetPath, texView.texture->image->uri);
        check(Mist::LoadTextureFromFile(context, texturePath, texOut, format));
        loadmeshlogf("Load texture: %s\n", texView.texture->image->uri);
        (*texOut)->CreateView(context, Mist::tViewDescription());
        return true;
    }
#endif // 0


    template <typename T>
    inline Mist::index_t GetArrayElementOffset(const T* root, const T* item) { check(item >= root); return Mist::index_t(item - root); }

#if 0
    void LoadMaterial(Mist::cMaterial& material, const Mist::RenderContext& context, const cgltf_material& cgltfmtl, const char* rootAssetPath)
    {
        material.m_flags = Mist::MATERIAL_FLAG_NONE;
        // Emissive
        if (cgltfmtl.has_emissive_strength)
        {
            material.m_flags |= Mist::MATERIAL_FLAG_EMISSIVE;
            ToVec3(material.m_emissiveFactor, cgltfmtl.emissive_factor);
            material.m_emissiveStrength = cgltfmtl.emissive_strength.emissive_strength;
            if (LoadTexture(context, rootAssetPath, cgltfmtl.emissive_texture, Mist::FORMAT_R8G8B8A8_SRGB, &material.m_textures[Mist::MATERIAL_TEXTURE_EMISSIVE]))
            {
                material.m_flags |= Mist::MATERIAL_FLAG_HAS_EMISSIVE_MAP;
            }
            else
                logfwarn("Emissive material without texture: %s\n", material.GetName());
        }
        // Metallic roughness
        if (cgltfmtl.has_pbr_metallic_roughness)
        {
            material.m_metallicFactor = cgltfmtl.pbr_metallic_roughness.metallic_factor;
            material.m_roughnessFactor = cgltfmtl.pbr_metallic_roughness.roughness_factor;
            if (LoadTexture(context, rootAssetPath, cgltfmtl.pbr_metallic_roughness.metallic_roughness_texture, Mist::FORMAT_R8G8B8A8_UNORM, &material.m_textures[Mist::MATERIAL_TEXTURE_METALLIC_ROUGHNESS]))
            {
                material.m_flags |= Mist::MATERIAL_FLAG_HAS_METALLIC_ROUGHNESS_MAP;
            }
            else
                logfwarn("Metallic roughness material without texture: %s\n", material.GetName());
        }
        // Unlit
        if (cgltfmtl.unlit)
        {
            material.m_flags |= Mist::MATERIAL_FLAG_UNLIT;
        }
        // Normal
        if (LoadTexture(context, rootAssetPath, cgltfmtl.normal_texture, Mist::FORMAT_R8G8B8A8_UNORM, &material.m_textures[Mist::MATERIAL_TEXTURE_NORMAL]))
        {
            material.m_flags |= Mist::MATERIAL_FLAG_HAS_NORMAL_MAP;
        }
        // Albedo
        ToVec3(material.m_albedo, cgltfmtl.pbr_metallic_roughness.base_color_factor);
        if (LoadTexture(context, rootAssetPath, cgltfmtl.pbr_metallic_roughness.base_color_texture, Mist::FORMAT_R8G8B8A8_SRGB, &material.m_textures[Mist::MATERIAL_TEXTURE_ALBEDO]))
            material.m_flags |= Mist::MATERIAL_FLAG_HAS_EMISSIVE_MAP;
    }
#else
    void LoadMaterial(rendersystem::Material* material, const cgltf_material& cgltfmtl, const char* rootAssetPath)
    {
        //material.m_flags = Mist::MATERIAL_FLAG_NONE;
        //// Emissive
        //if (cgltfmtl.has_emissive_strength)
        //{
        //    material.m_flags |= Mist::MATERIAL_FLAG_EMISSIVE;
        //    ToVec3(material.m_emissiveFactor, cgltfmtl.emissive_factor);
        //    material.m_emissiveStrength = cgltfmtl.emissive_strength.emissive_strength;
        //    if (LoadTexture(context, rootAssetPath, cgltfmtl.emissive_texture, Mist::FORMAT_R8G8B8A8_SRGB, &material.m_textures[Mist::MATERIAL_TEXTURE_EMISSIVE]))
        //    {
        //        material.m_flags |= Mist::MATERIAL_FLAG_HAS_EMISSIVE_MAP;
        //    }
        //    else
        //        logfwarn("Emissive material without texture: %s\n", material.GetName());
        //}
        //// Metallic roughness
        //if (cgltfmtl.has_pbr_metallic_roughness)
        //{
        //    material.m_metallicFactor = cgltfmtl.pbr_metallic_roughness.metallic_factor;
        //    material.m_roughnessFactor = cgltfmtl.pbr_metallic_roughness.roughness_factor;
        //    if (LoadTexture(context, rootAssetPath, cgltfmtl.pbr_metallic_roughness.metallic_roughness_texture, Mist::FORMAT_R8G8B8A8_UNORM, &material.m_textures[Mist::MATERIAL_TEXTURE_METALLIC_ROUGHNESS]))
        //    {
        //        material.m_flags |= Mist::MATERIAL_FLAG_HAS_METALLIC_ROUGHNESS_MAP;
        //    }
        //    else
        //        logfwarn("Metallic roughness material without texture: %s\n", material.GetName());
        //}
        //// Unlit
        //if (cgltfmtl.unlit)
        //{
        //    material.m_flags |= Mist::MATERIAL_FLAG_UNLIT;
        //}
        //// Normal
        //if (LoadTexture(context, rootAssetPath, cgltfmtl.normal_texture, Mist::FORMAT_R8G8B8A8_UNORM, &material.m_textures[Mist::MATERIAL_TEXTURE_NORMAL]))
        //{
        //    material.m_flags |= Mist::MATERIAL_FLAG_HAS_NORMAL_MAP;
        //}
        //// Albedo
        //ToVec3(material.m_albedo, cgltfmtl.pbr_metallic_roughness.base_color_factor);
        //if (LoadTexture(context, rootAssetPath, cgltfmtl.pbr_metallic_roughness.base_color_texture, Mist::FORMAT_R8G8B8A8_SRGB, &material.m_textures[Mist::MATERIAL_TEXTURE_ALBEDO]))
        //    material.m_flags |= Mist::MATERIAL_FLAG_HAS_EMISSIVE_MAP;
    }
#endif // 0

    rendersystem::modelloader::VertexLayout GetVertexLayout(const cgltf_primitive& primitive)
    {
        rendersystem::modelloader::VertexLayout layout;

        // get how many and which attributes has this primitive.
        static_assert(rendersystem::modelloader::Attribute_Count <= 8);
        for (uint32_t i = 0; i < primitive.attributes_count; ++i)
        {
            const cgltf_attribute& attribute = primitive.attributes[i];
            switch (attribute.type)
            {
            case cgltf_attribute_type_position: layout.attributeMask |= (1 << rendersystem::modelloader::Attribute_Position); break;
            case cgltf_attribute_type_texcoord:
                if (!strcmp(attribute.name, "TEXCOORD0")) layout.attributeMask |= (1 << rendersystem::modelloader::Attribute_TexCoord0);
                if (!strcmp(attribute.name, "TEXCOORD1")) layout.attributeMask |= (1 << rendersystem::modelloader::Attribute_TexCoord1);
                break;
            case cgltf_attribute_type_normal: layout.attributeMask |= (1 << rendersystem::modelloader::Attribute_Normal); break;
            case cgltf_attribute_type_color: layout.attributeMask |= (1 << rendersystem::modelloader::Attribute_VertexColor); break;
            case cgltf_attribute_type_tangent: layout.attributeMask |= (1 << rendersystem::modelloader::Attribute_Tangent); break;
            case cgltf_attribute_type_invalid:
            case cgltf_attribute_type_custom:
            case cgltf_attribute_type_max_enum:
                unreachable_code();
            }
        }

        // iterate attributes and build layout
        uint32_t offset = 0;
        for (uint32_t i = 0; i < rendersystem::modelloader::Attribute_Count; ++i)
        {
            if (layout.attributeMask & (1 << i))
            {
                uint32_t attributeSize = sizeof(float) * rendersystem::modelloader::GetAttributeElementCount((rendersystem::modelloader::AttributeType)i);
                layout.offsets[i] = offset;
                offset += attributeSize;
            }
        }
        layout.size = offset;
        return layout;
    }
}


namespace rendersystem
{
    namespace modelloader
    {

        bool LoadModelFromFile(render::Device* device, const char* filepath, Model* outModel)
        {
            PROFILE_SCOPE_LOGF(ModelLoader_LoadFile, "ModelLoader_LoadFile (%s)", filepath);

            Mist::cAssetPath assetPath(filepath);
            cgltf_data* data = gltf_api::ParseFile(assetPath);

            char rootAssetPath[512];
            Mist::FileSystem::GetDirectoryFromFilepath(assetPath, rootAssetPath, 512);
            if (!data)
            {
                logferror("Cannot open file to load scene model: %s.\n", assetPath);
                return false;
            }

            if (!data->nodes_count)
            {
                logferror("Model file without nodes in scene: %s.\n", assetPath);
                gltf_api::FreeData(data);
                return false;
            }
            //SetName(assetPath);

            loadmeshlog("=== Begin loading model ===\n");
            loadmeshlogf("Loading model from file: %s\n", assetPath);
            loadmeshlogf("* nodes:		%4d\n", data->nodes_count);
            loadmeshlogf("* materials:	%4d\n", data->materials_count);
            loadmeshlogf("* meshes:		%4d\n", data->meshes_count);
            loadmeshlogf("* textures:	%4d\n", data->textures_count);

            render::utils::UploadContext uploadContext(device);

            loadmeshlog("Loading materials\n");
            if (data->materials_count)
            {
                // init materials
                outModel->materials.Reserve((uint32_t)data->materials_count);
                // load materials
                for (uint32_t i = 0; i < outModel->materials.count; ++i)
                {
                    //m_materials[i].SetName(data->materials[i].name && *data->materials[i].name ? data->materials[i].name : "unknown");
                    gltf_api::LoadMaterial(&outModel->materials.data[i], data->materials[i], rootAssetPath);
                    //m_materials[i].SetupShader(context);
                }
            }
            else
            {
                logfwarn("Model without materials: %s\n", assetPath);
                outModel->materials.Reserve(1);
                // TODO assign default material
            }

            //InitNodes((index_t)data->nodes_count);
            //InitMeshes((index_t)data->meshes_count);
            outModel->meshes.Reserve((uint32_t)data->meshes_count);
            HeapArray<float> buffer;
            HeapArray<uint32_t> indices;
            VertexLayout vertexLayout;
            
            loadmeshlog("Loading meshes\n");
            for (uint32_t i = 0; i < outModel->meshes.count; ++i)
            {
                const cgltf_mesh& mesh = data->meshes[i];
                Mesh& outMesh = outModel->meshes.data[i];
                // init primitives
                outMesh.primitives.Reserve(Mist::limits_cast<uint32_t>(mesh.primitives_count));

                // first iterate primitives to get the total num of vertices and get vertex layout
                uint32_t vertexCount = 0;
                uint32_t indexCount = 0;
                for (uint32_t primitiveIndex = 0; primitiveIndex < mesh.primitives_count; ++primitiveIndex)
                {
                    const cgltf_primitive& primitive = mesh.primitives[primitiveIndex];
                    check(primitive.attributes_count == mesh.primitives[0].attributes_count);
                    check(primitive.type == cgltf_primitive_type_triangles);
                    check(primitive.indices && primitive.indices->type == cgltf_type_scalar && primitive.indices->count % 3 == 0);
                    check(primitive.attributes && primitive.attributes->data);
                    vertexCount += Mist::limits_cast<uint32_t>(primitive.attributes[0].data->count);
                    indexCount += Mist::limits_cast<uint32_t>(primitive.indices->count);

                    // Get vertices layout
                    if (vertexLayout.attributeMask == 0)
                        vertexLayout = gltf_api::GetVertexLayout(mesh.primitives[primitiveIndex]);
                    else
                    {
                        VertexLayout vl = gltf_api::GetVertexLayout(mesh.primitives[primitiveIndex]);
                        check(vl.attributeMask == vertexLayout.attributeMask && vl.size == vertexLayout.size);
                    }
                }
                // create buffers to read vertices and indices of this mesh
                check(vertexLayout.attributeMask && vertexLayout.size);
                uint32_t bufferSize = vertexCount * vertexLayout.size;
                buffer.Reserve(vertexCount);
                indices.Reserve(indexCount);

                loadmeshlogf("* mesh: %s (%d) [primitives: %d; indices: %d; vertices: %d: tris: %d]\n", mesh.name, i, 
                    mesh.primitives_count, indexCount, vertexCount, indexCount / 3);

                uint32_t indexOffset = 0;
                uint32_t vertexOffset = 0;
                // second iteration to store geometry in temporal buffers
                for (uint32_t primitiveIndex = 0; primitiveIndex < mesh.primitives_count; ++primitiveIndex)
                {
                    // load vertices and indices into temporal buffers
                    const cgltf_primitive& primitive = mesh.primitives[primitiveIndex];
                    gltf_api::LoadIndices(primitive, indices.data + indexOffset, vertexOffset);
                    gltf_api::LoadVertices(primitive, buffer.data + vertexOffset, bufferSize * sizeof(float), vertexLayout);

                    // build primitive object into mesh primitives buffer
                    Primitive& outPrimitive = outMesh.primitives.data[primitiveIndex];
                    outPrimitive.first = indexOffset;
                    outPrimitive.count = Mist::limits_cast<uint32_t>(primitive.indices->count);
                    outPrimitive.mesh = i;
                    check(primitive.material >= data->materials);
                    outPrimitive.material = Mist::limits_cast<uint32_t>(size_t(primitive.material) - size_t(data->materials));

                    indexOffset += Mist::limits_cast<uint32_t>(primitive.indices->count);
                    vertexOffset += Mist::limits_cast<uint32_t>(primitive.attributes->data->count);
                    check(indexOffset <= indexCount && vertexOffset <= vertexCount);
                }

                // build mesh object
                render::VertexInputAttribute attributes[rendersystem::modelloader::Attribute_Count];
                uint32_t attributeCount = 0;
                for (uint32_t j = 0; j < rendersystem::modelloader::Attribute_Count; ++j)
                {
                    if (vertexLayout.attributeMask & (1 << i))
                        attributes[attributeCount++] = { GetAttributeFormat((AttributeType)i) };
                }
                outMesh.inputLayout = render::VertexInputLayout::BuildVertexInputLayout(attributes, attributeCount);
                outMesh.vb = render::utils::CreateVertexBuffer(device, buffer.data, buffer.count * sizeof(float), &uploadContext);
                outMesh.ib = render::utils::CreateIndexBuffer(device, indices.data, indices.count * sizeof(uint32_t), &uploadContext);
            }
            uploadContext.Submit();
            loadmeshlog("=== End loading model ===\n");
            gltf_api::FreeData(data);
            return true;

#if 0

            tDynArray<Vertex> tempVertices;
            tDynArray<uint32_t> tempIndices;
            for (index_t i = 0; i < (index_t)data->nodes_count; ++i)
            {
                const cgltf_node& node = data->nodes[i];
                index_t parentIndex = index_invalid;
                if (node.parent)
                    parentIndex = gltf_api::GetArrayElementOffset(data->nodes, node.parent);
                index_t nodeIndex = BuildNode(i, parentIndex, node.name);

                // Process transform
                gltf_api::ReadNodeLocalTransform(node, m_transforms[i]);
                glm::vec3 pos, rot, scl;
                math::DecomposeMatrix(m_transforms[i], pos, rot, scl);
                loadmeshlogf("node %4d %s child of %4d\n", i, m_nodeNames[i].CStr(), parentIndex);
                loadmeshlogf("node %4d %s [pos %4.3f, %4.3f, %4.3f][rot %2.3f, %2.3f, %2.3f][scl %2.3f, %2.3f, %2.3f]\n", i, m_nodeNames[i].CStr(),
                    pos.x, pos.y, pos.z, rot.x, rot.y, rot.z, scl.x, scl.y, scl.z);

                // Process mesh
                if (node.mesh)
                {
                    index_t meshIndex = CreateMesh();
                    m_nodes[nodeIndex].MeshId = meshIndex;
                    cMesh& mesh = m_meshes[meshIndex];
                    mesh.SetName(node.mesh->name && *node.mesh->name ? node.mesh->name : "unknown");

                    loadmeshlogf("node %d %s has mesh %s\n", i, m_nodeNames[i].CStr(), mesh.GetName());

                    tFixedHeapArray<PrimitiveMeshData>& primitives = mesh.PrimitiveArray;
                    primitives.Allocate((index_t)node.mesh->primitives_count);
                    primitives.Resize((index_t)node.mesh->primitives_count);
                    loadmeshlogf("* primitives: %d\n", node.mesh->primitives_count);
                    for (uint32_t j = 0; j < node.mesh->primitives_count; ++j)
                    {
                        const cgltf_primitive& cgltfprimitive = node.mesh->primitives[j];
                        PrimitiveMeshData& primitive = primitives[j];
                        check(cgltfprimitive.type == cgltf_primitive_type_triangles);
                        check(cgltfprimitive.indices && cgltfprimitive.indices->type == cgltf_type_scalar && cgltfprimitive.indices->count % 3 == 0);
                        check(cgltfprimitive.attributes && cgltfprimitive.attributes->data);

                        uint32_t indexCount = (uint32_t)cgltfprimitive.indices->count;
                        uint32_t vertexCount = (uint32_t)cgltfprimitive.attributes[0].data->count;
                        uint32_t vertexOffset = (uint32_t)tempVertices.size();
                        uint32_t indexOffset = (uint32_t)tempIndices.size();
                        tempIndices.resize(indexOffset + indexCount);
                        tempVertices.resize(vertexOffset + vertexCount);

                        loadmeshlogf("** primitive %2d: [vertices %6d | %6d bytes][indices %4d | %6d bytes]\n", j, vertexCount, sizeof(Vertex) * vertexCount, indexCount, sizeof(uint32_t) * indexCount);

                        primitive.FirstIndex = indexOffset;
                        primitive.Count = indexCount;

                        gltf_api::LoadIndices(cgltfprimitive, tempIndices.data() + indexOffset, vertexOffset);
                        gltf_api::LoadVertices(cgltfprimitive, tempVertices.data() + vertexOffset, vertexCount);

                        primitive.RenderFlags = RenderFlags_Fixed;
                        if (cgltfprimitive.material)
                        {
                            index_t materialIndex = gltf_api::GetArrayElementOffset(data->materials, cgltfprimitive.material);
                            check(materialIndex < m_materials.GetSize());
                            cMaterial* material = &m_materials[materialIndex];
                            primitive.Material = material;
                            if (!(material->m_flags & MATERIAL_FLAG_NO_PROJECT_SHADOWS))
                                primitive.RenderFlags |= RenderFlags_ShadowMap;
                            if (material->m_flags & MATERIAL_FLAG_EMISSIVE)
                                primitive.RenderFlags |= RenderFlags_Emissive;
                        }
                        else
                        {
                            logfwarn("Primitive mesh without material: %s (Primitive %d)\n", mesh.GetName(), j);
                            check(!m_materials.IsEmpty());
                            primitive.Material = &m_materials[0];
                        }

                        check(cgltfprimitive.indices->count == primitive.Count);
                    }

                    //BuildTangents(tempVertices.data(), tempVertices.size(), tempIndices.data(), tempIndices.size());

                    mesh.SetupIndexBuffer(context, tempIndices.data(), (uint32_t)tempIndices.size());
                    mesh.SetupVertexBuffer(context, tempVertices.data(), (uint32_t)tempVertices.size() * sizeof(Vertex));
                    tempIndices.clear();
                    tempVertices.clear();
                }
                else
                    loadmeshlogf("node %d %s has no mesh\n", i, m_nodeNames[i].CStr());
            }
            loadmeshlog("=== End loading model ===\n");
            gltf_api::FreeData(data);
            return true;
#endif // 0

        }
}
}