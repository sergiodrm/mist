#pragma once

#include "Vertex.h"
#include "RenderHandle.h"
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

		inline RenderHandle GetDiffuseTexture() const { return m_diffuseTextureHandle; }
		inline RenderHandle GetNormalTexture() const { return m_normalTextureHandle; }
		inline RenderHandle GetSpecularTexture() const { return m_specularTextureHandle; }
		inline void SetDiffuseTexture(RenderHandle texHandle) { m_diffuseTextureHandle = texHandle, m_dirty = true; }
		inline void SetNormalTexture(RenderHandle texHandle) { m_normalTextureHandle = texHandle; m_dirty = true; }
		inline void SetSpecularTexture(RenderHandle texHandle) { m_specularTextureHandle = texHandle; m_dirty = true; }
		inline bool IsDirty() const { return m_dirty; }

	private:
		bool m_dirty{ true };
		RenderHandle m_diffuseTextureHandle;
		RenderHandle m_normalTextureHandle;
		RenderHandle m_specularTextureHandle;
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

		void MoveVerticesFrom(std::vector<Vertex>& vertices);
		void MoveIndicesFrom(std::vector<uint32_t>& indices);

	private:
		std::vector<Vertex> m_vertices;
		std::vector<uint32_t> m_indices;
	};

}
