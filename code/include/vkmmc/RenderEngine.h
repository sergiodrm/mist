#pragma once

#include <cstdint>
#include <functional>
#include <glm/glm.hpp>

namespace vkmmc
{
	class IScene;
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
		// Singleton static methods
		static IRenderEngine* MakeInstance();
		static IRenderEngine* GetRenderEngine();
		template <typename T> 
		static T* GetRenderEngineAs() { return static_cast<T*>(GetRenderEngine()); }
		static void FreeRenderEngine();


		virtual ~IRenderEngine() = default;
		virtual bool Init(const InitializationSpecs& initSpec) = 0;
		virtual bool RenderProcess() = 0;
		virtual void Shutdown() = 0;

		virtual void UpdateSceneView(const glm::mat4& view, const glm::mat4& projection) = 0;

		/** Scene to draw. Engine does NOT own the scene. Delete of the scene is on the side of the app. */
		virtual IScene* GetScene() = 0;
		virtual const IScene* GetScene() const = 0;
		virtual void SetScene(IScene* scene) = 0; 
		
		virtual void UploadMesh(Mesh& mesh) = 0;
		virtual void UploadMaterial(Material& material) = 0;
		virtual RenderHandle LoadTexture(const char* filepath) = 0;
		virtual void AddImGuiCallback(std::function<void()>&& fn) = 0;
		virtual RenderHandle GetDefaultTexture() const = 0;
		virtual Material GetDefaultMaterial() const = 0;
	};

}