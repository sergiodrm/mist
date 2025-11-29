#pragma once

#include "Mesh.h"
#include "RenderResource.h"
#include "Core/Types.h"

namespace Mist
{
	class cMaterial;
	struct sMaterialRenderData;

	enum RenderPassTypeBit
	{
		RenderPass_Opaque = 0x01,
		RenderPass_Transparent = 0x02,
		RenderPass_ShadowMap = 0x04,
	};
	typedef uint8_t RenderPassType;
	inline bool IsGeometryPass(RenderPassType type) { return type == RenderPass_ShadowMap; }

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
		
		inline const glm::mat4& GetTransform(index_t nodeIndex) const { return m_transforms[nodeIndex]; }
		inline index_t GetTransformsCount() const { return m_transforms.GetSize(); }
		inline const cMaterial& GetMaterial(index_t index) const { return m_materials[index]; }
		inline index_t GetMaterialCount() const { return m_materials.GetSize(); }
		void UpdateRenderTransforms(glm::mat4* globalTransforms, const glm::mat4& worldTransform) const;
		void UpdateMaterials(sMaterialRenderData* materials) const;
		index_t GetRoot() const { return m_root; }
		index_t GetNodeIndex(const sNode* node) const { check(m_nodes.GetData() <= node); return node - m_nodes.GetData(); }
		const sNode* GetNode(index_t nodeIndex) const { return nodeIndex < m_nodes.GetSize() ? &m_nodes[nodeIndex] : nullptr; }
		const char* GetNodeName(index_t nodeIndex) const { return nodeIndex < m_nodes.GetSize() ? m_nodeNames[nodeIndex].CStr() : nullptr; }
		inline index_t GetNodeFromMeshIndex(uint32_t meshIndex) const { return m_meshNodeIndex[meshIndex]; }

		const cMesh& GetMesh(uint32_t index) const { return m_meshes[index]; }
		uint32_t GetMeshCount() const { return m_meshes.GetSize(); }

		inline const AABB_t& GetAABB() const { return m_aabb; }
		inline uint8_t GetFlags() const { check(false); return 0; }
		void DumpInfo() const;
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
