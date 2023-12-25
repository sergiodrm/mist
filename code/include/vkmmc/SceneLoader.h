// header file for vkmmc project 
#pragma once

#include <vector>
#include "Vertex.h"

namespace vkmmc
{
	class Mesh;

	enum : uint32_t {SCENE_NODE_INVALID_ID = UINT32_MAX};
	struct SceneNode
	{
		uint32_t Id{SCENE_NODE_INVALID_ID};
		std::vector<uint32_t> Parents;
		std::vector<uint32_t> Children;
	};

	struct SceneNodeMesh
	{
		std::vector<Mesh> Meshes;
	};

	struct Scene
	{
		std::vector<SceneNode> Nodes;
		std::vector<SceneNodeMesh> Meshes;
	};

	class SceneLoader
	{
	public:
		static Scene LoadScene(const char* sceneFilepath);
	};
}