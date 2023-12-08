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
		inline RenderHandle GetHandle() const { return m_handle; }
		inline bool operator==(const RenderResource& r) const { return m_handle == r.GetHandle(); }
		inline bool operator!=(const RenderResource& r) const { return !(*this == r); }
	private:
		RenderHandle m_handle;
	};

	class Material : public RenderResource
	{
	public:

	private:
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
