
#include "Mesh.h"
#include "Core/Debug.h"

namespace Mist
{
	void cMesh::InitBuffers(render::Device* device, const void* vertices, uint32_t verticesSize, const uint32_t* indices, uint32_t indexCount)
	{
		m_indexCount = indexCount;
		m_vertexBuffer = render::utils::CreateVertexBuffer(device, vertices, verticesSize);
		m_indexBuffer = render::utils::CreateIndexBuffer(device, indices, indexCount * sizeof(uint32_t));
	}

	void cMesh::InitPrimitives(uint32_t count)
	{
		m_primitiveArray.AllocateAndResize(count);
	}

	void cMesh::Destroy()
	{
		m_vertexBuffer = nullptr;
		m_indexBuffer = nullptr;
		m_indexCount = 0;
		m_primitiveArray.Delete();
		m_renderFlags = 0;
		m_aabb.Invalidate();
	}
}