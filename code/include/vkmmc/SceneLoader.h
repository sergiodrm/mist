// header file for vkmmc project 
#pragma once

#include <vector>
#include "Vertex.h"

namespace vkmmc
{
	enum : uint32_t {SCENE_NODE_INVALID_ID = UINT32_MAX};
	struct SceneNode
	{
		uint32_t Id{SCENE_NODE_INVALID_ID};
		std::vector<uint32_t> Parents;
		std::vector<uint32_t> Children;
	};

	struct MeshGeometry
	{
		std::vector<Vertex> Vertices;
		std::vector<uint32_t> Indices;
	};

	struct SceneNodeMesh
	{
		std::vector<MeshGeometry> Geometries;
	};

	struct Scene
	{
		std::vector<SceneNode> Nodes;
		std::vector<SceneNodeMesh> Meshes;
	};

	class SceneLoader
	{
	public:
		static void LoadScene(const char* sceneFilepath);
	};
}