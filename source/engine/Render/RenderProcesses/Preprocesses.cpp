#include "Preprocesses.h"
#include "RenderSystem/RenderSystem.h"
#include "Render/VulkanRenderEngine.h"
#include "RenderSystem/TextureLoader.h"
#include "glm/ext/matrix_clip_space.inl"
#include "glm/ext/matrix_transform.inl"

namespace Mist
{

	void PreprocessIrradianceResources::Init(rendersystem::RenderSystem* rs)
	{
		rendersystem::ShaderBuildDescription shaderDesc;
		shaderDesc.vsDesc.filePath = "shaders/preprocess_equirectangular_cubemap.vert";
		shaderDesc.fsDesc.filePath = "shaders/preprocess_equirectangular_cubemap.frag";
		equirectangularShader = rs->CreateShader(shaderDesc);

		shaderDesc.fsDesc.filePath = "shaders/preprocess_irradiance_cubemap.frag";
		irradianceShader = rs->CreateShader(shaderDesc);

		render::TextureDescription texDesc;
		texDesc.extent = { 1024, 1024, 1 };
		texDesc.format = render::Format_R8G8B8A8_UNorm;
		texDesc.layers = 1;
		texDesc.isRenderTarget = true;
		texDesc.isShaderResource = true;
		render::TextureHandle rtTexture = rs->GetDevice()->CreateTexture(texDesc);

		render::RenderTargetDescription rtDesc;
		rtDesc.AddColorAttachment(rtTexture, render::TextureSubresourceRange(0, 1, 0, 1));
		rt = rs->GetDevice()->CreateRenderTarget(rtDesc);

		cubeModel.LoadModel(RenderContext(), ASSET_PATH("models/cube.gltf"));
	}

	void PreprocessIrradianceResources::Destroy(rendersystem::RenderSystem* rs)
	{
		cubeModel.Destroy(RenderContext());
		rt = nullptr;
		rs->DestroyShader(&equirectangularShader);
		rs->DestroyShader(&irradianceShader);
	}

	Preprocess::~Preprocess()
	{
	}

	void Preprocess::Init(const RenderContext& context)
	{
		m_irradianceResources.Init(g_render);
	}

	void Preprocess::Destroy(const RenderContext& context)
	{
		check(m_irradianceRequestCount == 0);
		m_irradianceResources.Destroy(g_render);
	}

	void Preprocess::Draw(const RenderContext& context, const RenderFrameContext& frameContext)
	{
		// flush irradiance processes
		for (uint32_t i = 0; i < m_irradianceRequestCount; ++i)
		{
			const PreprocessIrradianceInfo* info = m_irradianceInfoArray[i];
			fnPreprocessIrradianceCallback fn = m_irradianceFnArray[i];
			check(info && fn);
			// invalidate request
			m_irradianceInfoArray[i] = nullptr;
			m_irradianceFnArray[i] = nullptr;

			// process
			PreprocessIrradianceResult result = ProcessIrradianceRequest(*info);
			// notify
			fn(result, info->userData);
		}
		m_irradianceRequestCount = 0;
	}

	void Preprocess::PushIrradiancePreprocess(const PreprocessIrradianceInfo* info, fnPreprocessIrradianceCallback fn)
	{
		check(m_irradianceRequestCount < MaxRequests);
		check(info && fn);
		uint32_t index = m_irradianceRequestCount++;
		m_irradianceInfoArray[index] = info;
		m_irradianceFnArray[index] = fn;
	}

	PreprocessIrradianceResult Preprocess::ProcessIrradianceRequest(const PreprocessIrradianceInfo& info)
	{
		rendersystem::RenderSystem* renderSystem = g_render;

		render::TextureDescription cubemapDesc;
		cubemapDesc.extent = { info.cubemapWidthHeight, info.cubemapWidthHeight, 1 };
		cubemapDesc.format = render::Format_R8G8B8A8_UNorm;
		cubemapDesc.layers = 6;
		cubemapDesc.dimension = render::ImageDimension_Cube;
		cubemapDesc.isShaderResource = true;
		cubemapDesc.debugName = "equirectangular_cubemap";
		render::TextureHandle cubemap = g_device->CreateTexture(cubemapDesc);

		render::TextureDescription irradianceDesc;
		irradianceDesc.extent = { info.irradianceCubemapWidthHeight, info.irradianceCubemapWidthHeight, 1 };
		irradianceDesc.format = render::Format_R8G8B8A8_UNorm;
		irradianceDesc.layers = 6;
		irradianceDesc.dimension = render::ImageDimension_Cube;
		irradianceDesc.isShaderResource = true;
		irradianceDesc.debugName = "irradiance_cubemap";
		render::TextureHandle irradianceCubemap = g_device->CreateTexture(irradianceDesc);

		render::TextureHandle hdrFileContent;
		check(rendersystem::textureloader::LoadHDRTextureFromFile(&hdrFileContent, renderSystem->GetDevice(), info.hdrFilepath));

		glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
		glm::mat4 captureViews[] =
		{
		   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, 1.0f,  0.0f)),
		   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f,  0.0f,  0.0f), glm::vec3(0.0f, 1.0f,  0.0f)),
		   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, 1.0f)),
		   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
		   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f,  1.0f), glm::vec3(0.0f, 1.0f,  0.0f)),
		   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f, -1.0f), glm::vec3(0.0f, 1.0f,  0.0f)),
		};

		renderSystem->BeginMarker("Preprocess HDR cubemap");
		renderSystem->BeginMarker("Equirectangular");

		CameraData cd;
		cd.Projection = captureProjection;
		for (uint32_t i = 0; i < 6; ++i)
		{
			renderSystem->SetShader(m_irradianceResources.equirectangularShader);
			renderSystem->SetDepthEnable(false, false);
			renderSystem->SetVertexBuffer(m_irradianceResources.cubeModel.m_meshes[0].vb);
			renderSystem->SetIndexBuffer(m_irradianceResources.cubeModel.m_meshes[0].ib);
			renderSystem->SetTextureSlot("u_map", hdrFileContent);
			renderSystem->SetRenderTarget(m_irradianceResources.rt);
			renderSystem->SetViewport(m_irradianceResources.rt->m_info.GetViewport());
			renderSystem->SetScissor(m_irradianceResources.rt->m_info.GetScissor());
			renderSystem->ClearColor();
			cd.InvView = captureViews[i];
			//cd.InvView = glm::inverse(captureViews[i]);
			cd.ViewProjection = captureProjection * captureViews[i];
			renderSystem->SetShaderProperty("u_camera", &cd, sizeof(cd));
			renderSystem->DrawIndexed(m_irradianceResources.cubeModel.m_meshes[0].indexCount);
			renderSystem->ClearState();

			render::CopyTextureInfo info;
			info.extent = cubemapDesc.extent;
			info.srcOffset = {};
			info.dstOffset = {};
			info.srcLayer = { 0, 0, 1 };
			info.dstLayer = { 0, i, 1 };
			renderSystem->CopyTextureToTexture(m_irradianceResources.rt->m_description.colorAttachments[0].texture, cubemap, &info, 1);
		}
		renderSystem->ClearState();
		renderSystem->SetTextureAsResourceBinding(cubemap);

		renderSystem->EndMarker();

		renderSystem->BeginMarker("Irradiance");

		for (uint32_t i = 0; i < 6; ++i)
		{
			renderSystem->SetShader(m_irradianceResources.irradianceShader);
			renderSystem->SetDepthEnable(false, false);
			renderSystem->SetVertexBuffer(m_irradianceResources.cubeModel.m_meshes[0].vb);
			renderSystem->SetIndexBuffer(m_irradianceResources.cubeModel.m_meshes[0].ib);
			renderSystem->SetTextureSlot("u_cubemap", cubemap);
			renderSystem->SetRenderTarget(m_irradianceResources.rt);
			renderSystem->SetViewport(0, 0, (float)irradianceCubemap->m_description.extent.width, (float)irradianceCubemap->m_description.extent.height);
			renderSystem->SetScissor(0, (float)irradianceCubemap->m_description.extent.width, 0, (float)irradianceCubemap->m_description.extent.height);
			renderSystem->ClearColor();
			cd.InvView = captureViews[i];
			//cd.InvView = glm::inverse(captureViews[i]);
			cd.ViewProjection = captureProjection * captureViews[i];
			renderSystem->SetShaderProperty("u_camera", &cd, sizeof(cd));
			renderSystem->DrawIndexed(m_irradianceResources.cubeModel.m_meshes[0].indexCount);
			renderSystem->ClearState();

			render::CopyTextureInfo info;
			info.extent = irradianceDesc.extent;
			info.srcOffset = {};
			info.dstOffset = {};
			info.srcLayer = { 0, 0, 1 };
			info.dstLayer = { 0, i, 1 };
			renderSystem->CopyTextureToTexture(m_irradianceResources.rt->m_description.colorAttachments[0].texture, irradianceCubemap, &info, 1);
		}
		renderSystem->ClearState();
		renderSystem->SetTextureAsResourceBinding(irradianceCubemap);

		renderSystem->EndMarker();
		renderSystem->EndMarker();

		PreprocessIrradianceResult res;
		res.cubemap = cubemap;
		res.irradianceCubemap = irradianceCubemap;
		return res;
	}
}