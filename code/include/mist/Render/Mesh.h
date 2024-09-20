#pragma once

#include "Vertex.h"
#include "RenderHandle.h"
#include "Core/Types.h"
#include <vector>
#include <string>

namespace Mist
{
	class RenderResource
	{
	public:
		void SetHandle(RenderHandle handle);
		inline RenderHandle GetHandle() const { return m_handle; }
		inline bool operator==(const RenderResource& r) const { return m_handle == r.GetHandle(); }
		inline bool operator!=(const RenderResource& r) const { return !(*this == r); }
		const void* GetInternalData() const { return m_internalData; }
		void SetInternalData(void* internalData) { m_internalData = internalData; }
		void FreeInternalData();
		inline const char* GetName() const { return m_name.c_str(); }
		inline void SetName(const char* str) { m_name = str; }
	private:
		RenderHandle m_handle;
		std::string m_name;
	protected:
		void* m_internalData{ nullptr };
	};

	class Material : public RenderResource
	{
	public:

		inline RenderHandle GetAlbedoTexture() const { return m_albedoTextureHandle; }
		inline RenderHandle GetNormalTexture() const { return m_normalTextureHandle; }
		inline RenderHandle GetSpecularTexture() const { return m_specularTextureHandle; }
		inline RenderHandle GetOcclusionTexture() const { return m_occlusionTextureHandle; }
		inline RenderHandle GetMetallicTexture() const { return m_metallicTextureHandle; }
		inline RenderHandle GetRoughnessTexture() const { return m_roughnessTextureHandle; }
		inline void SetAlbedoTexture(RenderHandle texHandle) { m_albedoTextureHandle = texHandle, m_dirty = true; }
		inline void SetNormalTexture(RenderHandle texHandle) { m_normalTextureHandle = texHandle; m_dirty = true; }
		inline void SetSpecularTexture(RenderHandle texHandle) { m_specularTextureHandle = texHandle; m_dirty = true; }
		inline void SetOcclusionTexture(RenderHandle texHandle) { m_occlusionTextureHandle = texHandle; m_dirty = true; }
		inline void SetMetallicTexture(RenderHandle texHandle) { m_occlusionTextureHandle = texHandle; m_dirty = true; }
		inline void SetRoughnessTexture(RenderHandle texHandle) { m_occlusionTextureHandle = texHandle; m_dirty = true; }
		inline float GetMetallic() const { return m_metallic; }
		inline float GetRoughness() const { return m_roughness; }
		inline void SetMetallic(float value) { m_metallic = value; m_dirty = true; }
		inline void SetRoughness(float value) { m_roughness = value; m_dirty = true; }
		inline bool IsDirty() const { return m_dirty; }
		inline void SetDirty(bool v) { m_dirty = v; }

	private:
		bool m_dirty{ true };
		float m_roughness = 0.f;
		float m_metallic = 0.f;
		RenderHandle m_albedoTextureHandle;
		RenderHandle m_normalTextureHandle;
		RenderHandle m_specularTextureHandle;
		RenderHandle m_occlusionTextureHandle;
		RenderHandle m_metallicTextureHandle;
		RenderHandle m_roughnessTextureHandle;
	};

	class Mesh : public RenderResource
	{
	public:
		const Vertex* GetVertices() const { return m_vertices.data(); }
		inline uint32_t GetVertexCount() const { return (uint32_t)m_vertices.size(); }
		void SetVertices(const Vertex* data, size_t count);
		const uint32_t* GetIndices() const { return m_indices.data(); }
		inline uint32_t GetIndexCount() const { return (uint32_t)m_indices.size(); }
		void SetIndices(const uint32_t* data, size_t count);

		void MoveVerticesFrom(tDynArray<Vertex>& vertices);
		void MoveIndicesFrom(tDynArray<uint32_t>& indices);

	private:
		tDynArray<Vertex> m_vertices;
		tDynArray<uint32_t> m_indices;
	};

}
