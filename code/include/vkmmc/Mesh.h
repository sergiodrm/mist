#pragma once

#include "Vertex.h"
#include "RenderHandle.h"
#include <vector>

namespace vkmmc
{
	class Mesh
	{
	public:

		const Vertex* GetVertices() const { return m_vertices.data(); }
		inline size_t GetVertexCount() const { return m_vertices.size(); }
		void SetVertices(const Vertex* data, size_t count);
		void SetHandle(RenderHandle handle);
		inline RenderHandle GetHandle() const { return m_handle; }

		bool operator==(const Mesh& m) const { return m_handle == m.m_handle; }
		bool operator!=(const Mesh& m) const { return m_handle != m.m_handle; }
	private:
		std::vector<Vertex> m_vertices;
		RenderHandle m_handle;
	};
	
}
