
#include "Mesh.h"
#include "RenderTypes.h"

namespace vkmmc
{
	void RenderResource::SetHandle(RenderHandle handle)
	{
		vkmmc_check(!m_handle.IsValid());
		m_handle = handle;
	}

	void Mesh::SetVertices(const Vertex* data, size_t count)
	{
		vkmmc_check(!GetHandle().IsValid());
		m_vertices.clear();
		m_vertices.resize(count);
		memcpy_s(m_vertices.data(), sizeof(Vertex) * count, data, sizeof(Vertex) * count);
	}

}