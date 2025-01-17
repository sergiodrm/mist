#pragma once

#include <cstdint>
#include <functional>
#include <glm/glm.hpp>

namespace Mist
{
	class Scene;
	class Mesh;
	class Material;
	struct RenderObject;
	struct RenderObjectTransform;
	struct Window;


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
		virtual bool Init(const Window& window) = 0;
		virtual bool RenderProcess() = 0;
		virtual void Shutdown() = 0;

		virtual void UpdateSceneView(const glm::mat4& view, const glm::mat4& projection) = 0;
		virtual void ReloadShaders() = 0;

		/** Scene to draw. Engine does NOT own the scene. Delete of the scene is on the side of the app. */
		virtual Scene* GetScene() = 0;
		virtual const Scene* GetScene() const = 0;
		virtual void SetScene(Scene* scene) = 0;
	};

}