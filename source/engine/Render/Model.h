#pragma once

#include "Mesh.h"
#include "RenderResource.h"
#include "Core/Types.h"

namespace Mist
{
	class cMaterial;
	struct sMaterialRenderData;

	class cModel : public cRenderResource<RenderResource_Model>
	{
	public:
		struct sNode
		{
			index_t MeshId = index_invalid;
			index_t Parent = index_invalid;
			index_t Child = index_invalid;
			index_t Sibling = index_invalid;
		};

		bool LoadModel(render::Device* device, const char* filepath);
		void Destroy();
		
		inline index_t GetTransformsCount() const { return m_nodes.GetSize(); }
		inline index_t GetMaterialCount() const { return m_materials.GetSize(); }
		void UpdateRenderTransforms(glm::mat4* globalTransforms, const glm::mat4& worldTransform) const;
		void UpdateMaterials(sMaterialRenderData* materials) const;
		void ImGuiDraw();
		index_t GetRoot() const { return m_root; }
		const sNode* GetNode(index_t nodeIndex) const { return nodeIndex < m_nodes.GetSize() ? &m_nodes[nodeIndex] : nullptr; }
		inline index_t GetNodeFromMeshIndex(uint32_t meshIndex) const { return m_meshNodeIndex[meshIndex]; }

		const cMesh& GetMesh(uint32_t index) const { return m_meshes[index]; }
		uint32_t GetMeshCount() const { return m_meshes.GetSize(); }

		inline const AABB_t& GetAABB() const { return m_aabb; }
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
	private:
		tFixedHeapArray<sNode> m_nodes;
		tFixedHeapArray<tFixedString<64>> m_nodeNames;
		tFixedHeapArray<cMesh> m_meshes;
		// indices to relate the meshes with their nodes.
		tFixedHeapArray<uint32_t> m_meshNodeIndex;
		tFixedHeapArray<cMaterial> m_materials;
		tFixedHeapArray<glm::mat4> m_transforms;
		AABB_t m_aabb;
	};
}
