#pragma once

#include <cstdint>
#include <functional>

namespace vkmmc
{
	class Mesh;
	struct RenderObject;
	struct RenderObjectTransform;
	struct RenderObjectMesh;

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
		virtual bool RenderProcess() = 0;
		virtual void Shutdown() = 0;

		virtual RenderObject NewRenderObject() = 0;
		virtual RenderObjectTransform* GetObjectTransform(RenderObject object) = 0;
		virtual RenderObjectMesh* GetObjectMesh(RenderObject object) = 0;
		virtual uint32_t GetObjectCount() const = 0;
		virtual void UploadMesh(Mesh& mesh) = 0;
		virtual void SetImGuiCallback(std::function<void()>&& fn) = 0;
	};

	IRenderEngine* NewRenderEngine();
	void FreeRenderEngine(IRenderEngine** engine);
}