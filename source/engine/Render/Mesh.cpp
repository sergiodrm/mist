
#include "Mesh.h"
#include "RenderAPI.h"
#include "Core/Debug.h"

namespace Mist
{
    void cMesh::BindBuffers(VkCommandBuffer cmd) const
    {
        VertexBuffer.Bind(cmd);
        IndexBuffer.Bind(cmd);
    }

    void cMesh::Destroy(const RenderContext& context)
    {
        VertexBuffer.Destroy(context);
        IndexBuffer.Destroy(context);
    }

    void cMesh::SetupVertexBuffer(const RenderContext& context, const void* vertices, uint32_t vertexSize)
    {
        BufferCreateInfo info{
            .Size = vertexSize,
            .Data = vertices
        };
        VertexBuffer.Init(context, info);
    }

    void cMesh::SetupIndexBuffer(const RenderContext& context, const uint32_t* indices, uint32_t _indexCount)
    {
        BufferCreateInfo info{
            .Size = indexCount * sizeof(indexCount),
            .Data = indices
        };
        IndexBuffer.Init(context, info);
        indexCount = _indexCount;
    }

	void cMesh::Destroy()
	{
		vb = nullptr;
		ib = nullptr;
		primitiveArray.Delete();
	}
}