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

	enum RenderPassTypeBit
	{
		RenderPass_None = 0x00,
		RenderPass_Opaque = 0x01,
		RenderPass_Transparent = 0x02,
		RenderPass_ShadowMap = 0x04,

		RenderPass_All = 0xff
	};
	typedef uint8_t RenderPassType;
	inline bool IsGeometryPass(RenderPassType type) { return type == RenderPass_ShadowMap; }

	struct PrimitiveMeshData
	{
		RenderPassType renderPassMask;
		uint32_t firstIndex;
		uint32_t count;
		cMaterial* material;
		AABB_t aabb;
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
		inline void SetAABB(const AABB_t& aabb) { m_aabb = aabb; }

		inline RenderPassType GetRenderPassMask() const { return m_renderPassMask; }
		inline void SetRenderPassMask(RenderPassType mask) { m_renderPassMask = mask; }
	private:
		render::BufferHandle m_vertexBuffer;
		render::BufferHandle m_indexBuffer;
		uint32_t m_indexCount;

		// Cached from primitive data
		RenderPassType m_renderPassMask{ RenderPass_None };
		tFixedHeapArray<PrimitiveMeshData> m_primitiveArray;
		AABB_t m_aabb;
	};
}
