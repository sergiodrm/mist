#pragma once

#include <cstdint>
#include <functional>
#include <glm/glm.hpp>

namespace vkmmc
{
	class Scene;
	class Mesh;
	class Material;
	struct RenderObject;
	struct RenderObjectTransform;
	struct RenderObjectMesh;
	struct RenderHandle;

	struct InitializationSpecs
	{
		uint32_t WindowWidth;
		uint32_t WindowHeight;
		char WindowTitle[32];
	};

	// Abstract class of main renderer
	class IRenderEngine
	{
	protected:
		IRenderEngine() = default;
	public:
		// Factory static methods
		static IRenderEngine* NewRenderEngine();
		static void FreeRenderEngine(IRenderEngine** engine);


		virtual ~IRenderEngine() = default;
		virtual bool Init(const InitializationSpecs& initSpec) = 0;
		virtual void RenderLoop() = 0;
		virtual bool RenderProcess() = 0;
		virtual void Shutdown() = 0;

		virtual void UpdateSceneView(const glm::mat4& view, const glm::mat4& projection) = 0;

		virtual Scene& GetScene() = 0;
		virtual const Scene& GetScene() const = 0;
		virtual void UploadMesh(Mesh& mesh) = 0;
		virtual void UploadMaterial(Material& material) = 0;
		virtual RenderHandle LoadTexture(const char* filepath) = 0;
		virtual void AddImGuiCallback(std::function<void()>&& fn) = 0;
		virtual RenderHandle GetDefaultTexture() const = 0;
		virtual Material GetDefaultMaterial() const = 0;
	};

}