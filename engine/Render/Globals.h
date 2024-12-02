// Autogenerated code for Mist project
// Header file

#pragma once

#define UNIFORM_ID_SCENE_MODEL_TRANSFORM_ARRAY "u_model"
#define UNIFORM_ID_SCENE_ENV_DATA "Environment"
#define UNIFORM_ID_TIME "Time"
#define UNIFORM_ID_SHADOW_MAP_VP "ShadowMapVP"
#define UNIFORM_ID_LIGHT_VP "LightVP"
#define UNIFORM_ID_CAMERA "Camera"


#define ASSET_ROOT_PATH //"../assets/"

#define ASSET_PATH(path) ASSET_ROOT_PATH path

#define SHADER_RUNTIME_COMPILATION
#ifdef SHADER_RUNTIME_COMPILATION
#define SHADER_ROOT_PATH ASSET_ROOT_PATH "shaders/"
#define SHADER_ADDITIONAL_EXTENSION
#else
#define SHADER_ROOT_PATH ASSET_ROOT_PATH "shaders/compiled/"
#define SHADER_ADDITIONAL_EXTENSION ".spv"
#endif // SHADER_RUNTIME_COMPILATION


#define SHADER_COMPUTE_FILE_EXTENSION ".comp" SHADER_ADDITIONAL_EXTENSION
#define SHADER_VERTEX_FILE_EXTENSION ".vert" SHADER_ADDITIONAL_EXTENSION
#define SHADER_FRAG_FILE_EXTENSION ".frag" SHADER_ADDITIONAL_EXTENSION
#define SHADER_FILEPATH(filepath) SHADER_ROOT_PATH filepath SHADER_ADDITIONAL_EXTENSION


namespace Mist
{
	namespace globals
	{
		// Assets reference
		extern const char* BasicVertexShader;
		extern const char* BasicFragmentShader;
		extern const char* LineVertexShader;
		extern const char* LineFragmentShader;
		extern const char* DepthVertexShader;
		extern const char* DepthFragmentShader;
		extern const char* QuadVertexShader;
		extern const char* QuadFragmentShader;
		extern const char* DepthQuadFragmentShader;
		extern const char* PostProcessVertexShader;
		extern const char* PostProcessFragmentShader;
		inline constexpr unsigned int MaxOverlappedFrames = 3;
		inline constexpr unsigned int MaxRenderObjects = 1000;
		inline constexpr unsigned int MaxShadowMapAttachments = 3;
	}
}
