// header file for vkmmc project 
#pragma once

#include <vector>
#include <string>
#include "Vertex.h"
#include "Scene.h"

namespace vkmmc
{
	class Mesh;
	class Material;
	class IRenderEngine;

	/*enum : uint32_t {SCENE_NODE_INVALID_ID = UINT32_MAX};
	struct SceneNode
	{
		uint32_t Id{SCENE_NODE_INVALID_ID};
		std::vector<uint32_t> Parents;
		std::vector<uint32_t> Children;
	};

	struct SceneNodeMesh
	{
		std::vector<Mesh> Meshes;
		std::vector<Material> Materials;
	};

	struct Scene
	{
		std::vector<SceneNode> Nodes;
		std::vector<SceneNodeMesh> Meshes;
	};*/


	class SceneLoader
	{
	public:
		static Scene LoadScene(IRenderEngine* engine, const char* sceneFilepath);
	};
}