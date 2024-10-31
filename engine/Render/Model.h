#pragma once

#include "Mesh.h"
#include "Material.h"
#include "RenderAPI.h"
#include "RenderResource.h"
#include "Core/Types.h"

namespace Mist
{
	class cModel : public cRenderResource
	{
		struct sNode
		{
			index_t MeshId = index_invalid;
			index_t Parent = index_invalid;
			index_t Child = index_invalid;
			index_t Sibling = index_invalid;
		};
	public:

		void LoadModel(const RenderContext& context, const char* filepath);
		void Destroy(const RenderContext& context);
		
		inline index_t GetTransformsCount() const { return m_nodes.GetSize(); }
		inline index_t GetMaterialCount() const { return m_materials.GetSize(); }
		void UpdateRenderTransforms(glm::mat4* globalTransforms, const glm::mat4& worldTransform) const;
		void UpdateMaterials(sMaterialRenderData* materials) const;
	private:
		void InitNodes(index_t n);
		void InitMeshes(index_t n);
		void InitMaterials(index_t n);

		void CalculateGlobalTransforms(glm::mat4* transforms, index_t node) const;

		index_t BuildNode(index_t nodeIndex, index_t parentIndex, const char* nodeName);
		sNode* GetNode(index_t i);

		index_t CreateMesh();
		index_t CreateMaterial();


		index_t m_root = index_invalid;
	public:
		tFixedHeapArray<sNode, index_t> m_nodes;
		tFixedHeapArray<tFixedString<32>, index_t> m_nodeNames;
		tFixedHeapArray<cMesh, index_t> m_meshes;
		tFixedHeapArray<cMaterial, index_t> m_materials;
		tFixedHeapArray<glm::mat4, index_t> m_transforms;
	};
}
