#pragma once

#include "Vertex.h"
#include "VulkanBuffer.h"
#include "RenderAPI.h"
#include "RenderResource.h"
#include "RenderProcesses/RenderProcess.h"
#include "Core/Types.h"
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
		PrimitiveMeshData() : RenderFlags(0), FirstIndex(0), Count(0), Material(nullptr) {}
	};

	class cMesh : public cRenderResource<RenderResource_Mesh>
	{
	public:
		void Destroy();

		render::BufferHandle vb;
		render::BufferHandle ib;
		uint32_t indexCount;
		eRenderFlags renderFlags;
		tFixedHeapArray<PrimitiveMeshData> primitiveArray;
	};
}
