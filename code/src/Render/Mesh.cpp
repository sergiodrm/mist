
#include "Render/Mesh.h"
#include "Core/Debug.h"

namespace Mist
{
	void RenderResource::SetHandle(RenderHandle handle)
	{
		check(!m_handle.IsValid());
		m_handle = handle;
	}

	void RenderResource::FreeInternalData()
	{
		if (m_internalData)
		{
			delete m_internalData;
			m_internalData = nullptr;
		}
	}

	void Mesh::SetVertices(const Vertex* data, size_t count)
	{
		check(!GetHandle().IsValid());
		m_vertices.clear();
		m_vertices.resize(count);
		memcpy_s(m_vertices.data(), sizeof(Vertex) * count, data, sizeof(Vertex) * count);
	}

	void Mesh::SetIndices(const uint32_t* data, size_t count)
	{
		check(!GetHandle().IsValid());
		m_indices.clear();
		m_indices.resize(count);
		memcpy_s(m_indices.data(), sizeof(uint32_t) * count, data, sizeof(uint32_t) * count);
	}

	void Mesh::MoveVerticesFrom(tDynArray<Vertex>& vertices)
	{
		m_vertices = std::move(vertices);
	}

	void Mesh::MoveIndicesFrom(tDynArray<uint32_t>& indices)
	{
		m_indices = std::move(indices);
	}

}