#pragma once

#include "Mesh.h"
#include "Material.h"
#include "RenderAPI.h"
#include "RenderResource.h"
#include "Core/Types.h"

namespace Mist
{
	class cModel : public cRenderResource<RenderResource_Model>
	{
		struct sNode
		{
			index_t MeshId = index_invalid;
			index_t Parent = index_invalid;
			index_t Child = index_invalid;
			index_t Sibling = index_invalid;
		};
	public:

		bool LoadModel(const RenderContext& context, const char* filepath);
		void Destroy(const RenderContext& context);
		
		inline index_t GetTransformsCount() const { return m_nodes.GetSize(); }
		inline index_t GetMaterialCount() const { return m_materials.GetSize(); }
		void UpdateRenderTransforms(glm::mat4* globalTransforms, const glm::mat4& worldTransform) const;
		void UpdateMaterials(sMaterialRenderData* materials) const;
		void ImGuiDraw();
	private:
		void InitNodes(index_t n);
		void InitMeshes(index_t n);
		void InitMaterials(index_t n);

		void CalculateGlobalTransforms(glm::mat4* transforms, index_t node) const;

		index_t BuildNode(index_t nodeIndex, index_t parentIndex, const char* nodeName);
		sNode* GetNode(index_t i);
		void SetNodeName(index_t i, const char* name);
		void SetNodeTransform(index_t i, const glm::mat4& transform);

		index_t CreateMesh();
		index_t CreateMaterial();

		void DumpInfo() const;


		index_t m_root = index_invalid;
	public:
		tFixedHeapArray<sNode> m_nodes;
		tFixedHeapArray<tFixedString<64>> m_nodeNames;
		tFixedHeapArray<cMesh> m_meshes;
		tFixedHeapArray<uint32_t> m_meshNodeIndex;
		tFixedHeapArray<cMaterial> m_materials;
		tFixedHeapArray<glm::mat4> m_transforms;
	};
}
