
#include "Model.h"
#include "ModelLoader.h"
#include "Core/Debug.h"
#include "Core/Logger.h"
#include "RenderProcesses/RenderProcess.h"
#include <imgui/imgui.h>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/fwd.hpp>
#include "VulkanRenderEngine.h"

namespace Mist
{
	void cModel::Destroy()
	{
		for (index_t i = 0; i < m_meshes.GetSize(); ++i)
			m_meshes[i].Destroy();
		for (index_t i = 0; i < m_materials.GetSize(); ++i)
			m_materials[i].Invalidate();
		m_nodes.Delete();
		m_meshes.Delete();
		m_materials.Delete();
		m_transforms.Delete();
		m_root = index_invalid;
	}

	bool cModel::LoadResource()
	{
		return modelloader::LoadModelFromFile(GetRenderDevice(), GetResourceName(), this);
	}

	void cModel::DestroyResource()
	{
		Destroy();
	}

	void cModel::UpdateRenderTransforms(glm::mat4* globalTransforms, const glm::mat4& worldTransform) const
	{
		check(m_root != index_invalid);
		globalTransforms[m_root] = worldTransform * m_transforms[m_root];
		check(m_nodes[m_root].Sibling == index_invalid);
		if (m_nodes[m_root].Child != index_invalid)
			CalculateGlobalTransforms(globalTransforms, m_nodes[m_root].Child);
	}

	void cModel::UpdateMaterials(sMaterialRenderData* materials) const
	{
		for (index_t i = 0; i < m_materials.GetSize(); ++i)
		{
			materials[i] = m_materials[i].GetRenderData();
		}
	}

	void cModel::InitNodes(index_t n)
	{
		// Add model root node
		++n;
		check(m_nodes.IsEmpty());
		m_nodes.Allocate(n);
		m_nodes.Resize(n);

		check(m_nodeNames.IsEmpty());
		m_nodeNames.Allocate(n);
		m_nodeNames.Resize(n);

		check(m_transforms.IsEmpty());
		m_transforms.Allocate(n);
		m_transforms.Resize(n);

		check(m_nodeMeshInfoArray.IsEmpty());
		m_nodeMeshInfoArray.Allocate(n);

		// Build default root node
		m_root = n - 1;
		Node* node = GetNode(m_root);
		check(node);
		SetNodeName(m_root, "_root_");
		SetNodeTransform(m_root, glm::mat4(1.f));
	}

	void cModel::InitMeshes(index_t n)
	{
		check(m_meshes.IsEmpty());
		m_meshes.Allocate(n);
		m_meshes.Resize(n);
	}

	void cModel::InitMaterials(index_t n)
	{
		check(m_materials.IsEmpty());
		m_materials.Allocate(n);
		m_materials.Resize(n);
	}

	void cModel::CalculateGlobalTransforms(glm::mat4* transforms, index_t node) const
	{
		check(node != index_invalid);
		const Node& n = m_nodes[node];

		// calculate current
		if (n.Parent != index_invalid)
			transforms[node] = transforms[n.Parent] * m_transforms[node];
		else
			transforms[node] = m_transforms[node];

		// calculate siblings
		if (n.Sibling != index_invalid)
			CalculateGlobalTransforms(transforms, n.Sibling);

		// calculate child
		if (n.Child != index_invalid)
			CalculateGlobalTransforms(transforms, n.Child);
	}

	index_t cModel::BuildNode(index_t nodeIndex, index_t parentIndex, const char* nodeName)
	{
		Node& node = m_nodes[nodeIndex];
		SetNodeName(nodeIndex, nodeName && *nodeName ? nodeName : "unknown");
		Node* nodeParent = GetNode(parentIndex);
		if (!nodeParent)
		{
#if 0
			if (m_root == index_invalid)
				m_root = nodeIndex;
			else
#endif // 0
			{
				check(m_root != index_invalid);
				Logf(LogLevel::Warn, "Node (%d %s) of scene has no parent after found a root node (%d %s) in scene graph: %s\n",
					nodeIndex, m_nodeNames[nodeIndex].CStr(), m_root, m_nodeNames[m_root].CStr(), GetResourceName());
				nodeParent = GetNode(m_root);
				parentIndex = m_root;
			}
		}

		if (nodeParent)
		{
			if (nodeParent->Child != index_invalid)
			{
				Node* sibling = GetNode(nodeParent->Child);
				for (; sibling->Sibling != index_invalid; sibling = GetNode(sibling->Sibling));
				check(sibling && sibling->Sibling == index_invalid);
				sibling->Sibling = nodeIndex;
			}
			else
				nodeParent->Child = nodeIndex;
		}
		node.Parent = parentIndex;
		return nodeIndex;
	}

	cModel::Node* cModel::GetNode(index_t i)
	{
		if (i < m_nodes.GetSize())
			return &m_nodes[i];
		return nullptr;
	}

	void cModel::SetNodeName(index_t i, const char* name)
	{
		check(i < m_nodeNames.GetSize());
		m_nodeNames[i] = name;
	}

	void cModel::SetNodeTransform(index_t i, const glm::mat4& transform)
	{
		check(i < m_transforms.GetSize());
		m_transforms[i] = transform;
	}

	index_t cModel::CreateMesh()
	{
		check(m_meshes.GetReservedSize());
		m_meshes.Push();
		return m_meshes.GetSize() - 1;
	}

	index_t cModel::CreateMaterial()
	{
		check(!m_materials.IsEmpty());
		m_materials.Push();
		return m_materials.GetSize() - 1;
	}

	index_t cModel::LinkNodeToMesh(index_t node, index_t meshId)
	{
		check(node < m_nodes.GetSize());
		check(meshId < m_meshes.GetSize());

		// Create new NodeMeshInfo and fill data with indices
		m_nodeMeshInfoArray.Push();
		NodeMeshInfo& nodeMeshInfo = m_nodeMeshInfoArray.Back();
		nodeMeshInfo.node = node;
		nodeMeshInfo.mesh = meshId;

		// Link node to new NodeMeshInfo
		check(m_nodes[node].meshInfoIndex == index_invalid);
		m_nodes[node].meshInfoIndex = m_nodeMeshInfoArray.GetSize() - 1;
		return m_nodeMeshInfoArray.GetSize() - 1;
	}

	void cModel::DumpInfo() const
	{
		index_t it = m_root;
		while (it != index_invalid)
		{
			const Node& node = m_nodes[it];
			logfinfo("* node: %d (%s) (Parent: %d)\n", it, m_nodeNames[it].CStr(), node.Parent);
			logfinfo("* mesh: %d\n", node.meshInfoIndex != index_invalid ? m_nodeMeshInfoArray[node.meshInfoIndex].mesh : -1);
			logfinfo("** transform: \n");
			const glm::mat4& m = m_transforms[it];
			logfinfo("** [%6.3f %6.3f %6.3f %6.3f]\n", m[0][0], m[1][0], m[2][0], m[3][0]);
			logfinfo("** [%6.3f %6.3f %6.3f %6.3f]\n", m[0][1], m[1][1], m[2][1], m[3][1]);
			logfinfo("** [%6.3f %6.3f %6.3f %6.3f]\n", m[0][2], m[1][2], m[2][2], m[3][2]);
			logfinfo("** [%6.3f %6.3f %6.3f %6.3f]\n", m[0][3], m[1][3], m[2][3], m[3][3]);

			if (node.Child != index_invalid)
				it = node.Child;
			else if (node.Sibling != index_invalid)
				it = node.Sibling;
			else if (node.Parent != index_invalid)
			{
				const Node& parent = m_nodes[node.Parent];
				if (parent.Sibling != index_invalid)
					it = parent.Sibling;
				else
					it = index_invalid;
			}
			else
				it = index_invalid;
		}
	}

	void cModel::MoveMaterials(tFixedHeapArray<cMaterial>&& materials)
	{
		m_materials = materials.Move();
	}
}