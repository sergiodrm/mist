#pragma once

#include "Mesh.h"
#include "RenderResource.h"
#include "Core/Types.h"

namespace Mist
{
	class cMaterial;
	struct sMaterialRenderData;

	enum StencilMaskBit
	{
		StencilMask_None = 0x00,
		StencilMask_Geometry = 0x01,
		StencilMask_Sky = 0x02,
		StencilMask_All = 0xff
	};
	typedef uint8_t StencilMask;

	class cModel : public cRenderResource<RenderResource_Model>
	{
	public:
		struct Node
		{
			index_t meshInfoIndex = index_invalid;
			index_t Parent = index_invalid;
			index_t Child = index_invalid;
			index_t Sibling = index_invalid;
		};
		struct NodeMeshInfo
		{
			index_t node = index_invalid;
			index_t mesh = index_invalid;

			inline bool IsValid() const { return node != index_invalid && mesh != index_invalid; }
		};

		bool LoadModel(render::Device* device, const char* filepath);
		void Destroy();
		
		inline const glm::mat4& GetTransform(index_t nodeIndex) const { return m_transforms[nodeIndex]; }
		inline index_t GetTransformsCount() const { return m_transforms.GetSize(); }
		inline const cMaterial& GetMaterial(index_t index) const { return m_materials[index]; }
		inline index_t GetMaterialCount() const { return m_materials.GetSize(); }
		void UpdateRenderTransforms(glm::mat4* globalTransforms, const glm::mat4& worldTransform) const;
		void UpdateMaterials(sMaterialRenderData* materials) const;
		index_t GetRoot() const { return m_root; }
		index_t GetNodeIndex(const Node* node) const { check(m_nodes.GetData() <= node); return node - m_nodes.GetData(); }
		const Node* GetNode(index_t nodeIndex) const { return nodeIndex < m_nodes.GetSize() ? &m_nodes[nodeIndex] : nullptr; }
		const char* GetNodeName(index_t nodeIndex) const { return nodeIndex < m_nodes.GetSize() ? m_nodeNames[nodeIndex].CStr() : nullptr; }

		const cMesh* GetMeshFromNode(index_t node) const { return (node < m_nodes.GetSize() && m_nodes[node].meshInfoIndex != index_invalid) ? &m_meshes[m_nodeMeshInfoArray[m_nodes[node].meshInfoIndex].mesh] : nullptr; }
		const cMesh& GetMesh(uint32_t index) const { return m_meshes[index]; }
		uint32_t GetMeshCount() const { return m_meshes.GetSize(); }

		inline const AABB_t& GetAABB() const { return m_aabb; }
		inline RenderPassType GetRenderPassMask() const { return m_renderPassMask; }
		void DumpInfo() const;
	private:
		void InitNodes(index_t n);
		void InitMeshes(index_t n);
		void InitMaterials(index_t n);

		void CalculateGlobalTransforms(glm::mat4* transforms, index_t node) const;

		index_t BuildNode(index_t nodeIndex, index_t parentIndex, const char* nodeName);
		Node* GetNode(index_t i);
		void SetNodeName(index_t i, const char* name);
		void SetNodeTransform(index_t i, const glm::mat4& transform);

		index_t CreateMesh();
		index_t CreateMaterial();
		index_t LinkNodeToMesh(index_t node, index_t meshId);

		index_t m_root = index_invalid;
	private:
		// All nodes must have the basic data: Node info hierarchy, name and transform
		tFixedHeapArray<Node> m_nodes;
		tFixedHeapArray<tFixedString<64>> m_nodeNames;
		tFixedHeapArray<glm::mat4> m_transforms;

		// Meshes. Several nodes can point to the same mesh.
		tFixedHeapArray<cMesh> m_meshes;
		// Materials used by meshes. Several meshes can use the same material.
		tFixedHeapArray<cMaterial> m_materials;

		// indices to relate the meshes with their nodes.
		tFixedHeapArray<NodeMeshInfo> m_nodeMeshInfoArray;

		// Cached info. Computed at model creation. Must be constant after loading.
		AABB_t m_aabb;
		RenderPassType m_renderPassMask{RenderPass_None};
	};
}
