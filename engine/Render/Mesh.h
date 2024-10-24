#pragma once

#include "Vertex.h"
#include "VulkanBuffer.h"
#include "RenderAPI.h"
#include "RenderResource.h"
#include "Core/Types.h"
#include <vector>
#include <string>

namespace Mist
{
	class cMaterial;
	struct PrimitiveMeshData
	{
		uint32_t FirstIndex;
		uint32_t Count;
		cMaterial* Material;
		PrimitiveMeshData() : FirstIndex(0), Count(0), Material(nullptr) {}
	};

	class cMesh : public cRenderResource
	{
	public:
		void BindBuffers(CommandBuffer cmd) const;
		void Destroy(const RenderContext& context);
		void SetupVertexBuffer(const RenderContext& context, const void* vertices, uint32_t vertexSize);
		void SetupIndexBuffer(const RenderContext& context, const uint32_t* indices, uint32_t indexCount);

		VertexBuffer VertexBuffer;
		IndexBuffer IndexBuffer;
		uint32_t IndexCount;
		tDynArray<PrimitiveMeshData> PrimitiveArray;
	};
}
