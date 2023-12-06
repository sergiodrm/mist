
#include "Mesh.h"
#include "RenderTypes.h"

namespace vkmmc
{

	void Mesh::SetVertices(const Vertex* data, size_t count)
	{
		vkmmc_check(!m_handle.IsValid());
		m_vertices.clear();
		m_vertices.resize(count);
		memcpy_s(m_vertices.data(), sizeof(Vertex) * count, data, sizeof(Vertex) * count);
	}

	void Mesh::SetHandle(RenderHandle handle)
	{
		vkmmc_check(!m_handle.IsValid());
		m_handle = handle;
	}
}