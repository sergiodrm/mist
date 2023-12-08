#pragma once

#include <cstdint>

namespace vkmmc
{
	class Mesh;
	class RenderObject;

	struct InitializationSpecs
	{
		uint32_t WindowWidth;
		uint32_t WindowHeight;
		char WindowTitle[32];
	};
	// Abstract class of main renderer
	class IRenderEngine
	{
	public:
		virtual ~IRenderEngine() = default;
		virtual bool Init(const InitializationSpecs& initSpec) = 0;
		virtual void RenderLoop() = 0;
		virtual void Shutdown() = 0;

		virtual void AddRenderObject(RenderObject object) = 0;
		virtual void UploadMesh(Mesh& mesh) = 0;
	};

	IRenderEngine* NewRenderEngine();
	void FreeRenderEngine(IRenderEngine** engine);
}