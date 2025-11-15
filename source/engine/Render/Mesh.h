#pragma once

#include "Vertex.h"
#include "RenderResource.h"
#include "Culling.h"
#include "RenderProcesses/RenderProcess.h"
#include "Core/Types.h"
#include "Utils/GenericUtils.h"
#include <vector>
#include <string>

#include "RenderAPI/Device.h"

namespace Mist
{
	class cMaterial;

	struct PrimitiveMeshData
	{
		uint16_t RenderFlags;
		uint32_t FirstIndex;
		uint32_t Count;
		cMaterial* Material;
		AABB_t AABB;
	};

	class cMesh : public cRenderResource<RenderResource_Mesh>
	{
	public:
		void InitBuffers(render::Device* device, const void* vertices, uint32_t verticesSize, const uint32_t* indices, uint32_t indexCount);
		void InitPrimitives(uint32_t count);
		void Destroy();

		inline const render::BufferHandle& GetVertexBuffer() const { return m_vertexBuffer; }
		inline const render::BufferHandle& GetIndexBuffer() const { return m_indexBuffer; }
		inline uint32_t GetIndexCount() const { return m_indexCount; }
		inline const PrimitiveMeshData* GetPrimitiveArray() const { return m_primitiveArray.GetData(); }
		inline PrimitiveMeshData* GetPrimitiveArray() { return m_primitiveArray.GetData(); }
		inline uint32_t GetPrimitiveCount() const { return m_primitiveArray.GetSize(); }
		inline const AABB_t& GetAABB() const { return m_aabb; }
		inline uint32_t GetRenderFlags() const { return m_renderFlags; }

		inline void SetAABB(const AABB_t& aabb) { m_aabb = aabb; }
		inline void ActivateRenderFlags(uint32_t flags) { m_renderFlags |= flags; }
		inline void DeactivateRenderFlags(uint32_t flags) { m_renderFlags &= ~flags; }
		inline void SetRenderFlags(uint32_t flags) { m_renderFlags = flags; }
	private:
		render::BufferHandle m_vertexBuffer;
		render::BufferHandle m_indexBuffer;
		uint32_t m_indexCount;
		uint32_t m_renderFlags;
		tFixedHeapArray<PrimitiveMeshData> m_primitiveArray;
		AABB_t m_aabb;
	};
}
