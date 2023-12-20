#pragma once

#include "Vertex.h"
#include "RenderHandle.h"
#include <vector>

namespace vkmmc
{
	class RenderResource
	{
	public:
		void SetHandle(RenderHandle handle);
		inline RenderHandle GetHandle() const { return m_pipelineHandle; }
		inline bool operator==(const RenderResource& r) const { return m_pipelineHandle == r.GetHandle(); }
		inline bool operator!=(const RenderResource& r) const { return !(*this == r); }
	private:
		RenderHandle m_pipelineHandle;
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
		inline size_t GetVertexCount() const { return m_vertices.size(); }
		void SetVertices(const Vertex* data, size_t count);
	private:
		std::vector<Vertex> m_vertices;
	};
	
}
