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

		shaderDesc.fsDesc.filePath = "shaders/preprocess_specular_cubemap.frag";
		specularShader = rs->CreateShader(shaderDesc);

		shaderDesc.vsDesc.filePath = "shaders/quad.vert";
		shaderDesc.fsDesc.filePath = "shaders/preprocess_brdf.frag";
		brdfShader = rs->CreateShader(shaderDesc);

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

	void PreprocessIrradianceResources::PrepareDraw(rendersystem::RenderSystem* rs, render::Extent2D viewportSize, rendersystem::ShaderProgram* shader)
	{
		rs->SetShader(shader);
		rs->SetDepthEnable(false, false);
		rs->SetVertexBuffer(cubeModel.m_meshes[0].vb);
		rs->SetIndexBuffer(cubeModel.m_meshes[0].ib);
		rs->SetRenderTarget(rt);
		rs->SetViewport(0.f, 0.f, (float)viewportSize.width, (float)viewportSize.height);
		rs->SetScissor(0.f, (float)viewportSize.width, 0.f, (float)viewportSize.height);
		rs->ClearColor();
	}

	void PreprocessIrradianceResources::DrawCube(rendersystem::RenderSystem* rs)
	{
		rs->DrawIndexed(cubeModel.m_meshes[0].indexCount);
	}

	void PreprocessIrradianceResources::Destroy(rendersystem::RenderSystem* rs)
	{
		cubeModel.Destroy(RenderContext());
		rt = nullptr;
		rs->DestroyShader(&equirectangularShader);
		rs->DestroyShader(&irradianceShader);
		rs->DestroyShader(&specularShader);
		rs->DestroyShader(&brdfShader);
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
			PreprocessIrradianceInfo& info = m_irradianceInfoArray[i];
			fnPreprocessIrradianceCallback fn = m_irradianceFnArray[i];
			check(fn);

			// process
			PreprocessIrradianceResult result = ProcessIrradianceRequest(info);
			// notify
			fn(result, info.userData);
			// invalidate request
			m_irradianceFnArray[i] = nullptr;
			info.hdrFilepath = 0;
			info.cubemapWidthHeight = 0;
			info.irradianceCubemapWidthHeight = 0;
			info.userData = nullptr;
		}
		m_irradianceRequestCount = 0;
	}

	void Preprocess::PushIrradiancePreprocess(const PreprocessIrradianceInfo& info, fnPreprocessIrradianceCallback fn)
	{
		check(m_irradianceRequestCount < MaxRequests);
		check(fn);
		uint32_t index = m_irradianceRequestCount++;
		m_irradianceInfoArray[index] = info;
		m_irradianceFnArray[index] = fn;
	}

	PreprocessIrradianceResult Preprocess::ProcessIrradianceRequest(const PreprocessIrradianceInfo& info)
	{
		rendersystem::RenderSystem* renderSystem = g_render;
		render::Device* device = renderSystem->GetDevice();

		// Create output resources
		render::TextureDescription cubemapDesc;
		cubemapDesc.extent = { info.cubemapWidthHeight, info.cubemapWidthHeight, 1 };
		cubemapDesc.format = render::Format_R8G8B8A8_UNorm;
		cubemapDesc.layers = 6;
		cubemapDesc.dimension = render::ImageDimension_Cube;
		cubemapDesc.isShaderResource = true;
		cubemapDesc.debugName = "equirectangular_cubemap";
		render::TextureHandle cubemap = device->CreateTexture(cubemapDesc);

		render::TextureDescription irradianceDesc;
		irradianceDesc.extent = { info.irradianceCubemapWidthHeight, info.irradianceCubemapWidthHeight, 1 };
		irradianceDesc.format = render::Format_R8G8B8A8_UNorm;
		irradianceDesc.layers = 6;
		irradianceDesc.dimension = render::ImageDimension_Cube;
		irradianceDesc.isShaderResource = true;
		irradianceDesc.debugName = "irradiance_cubemap";
		render::TextureHandle irradianceCubemap = device->CreateTexture(irradianceDesc);

		render::TextureDescription specularCubeDesc;
		specularCubeDesc.extent = { info.specularCubemapWidthHeight, info.specularCubemapWidthHeight, 1 };
		specularCubeDesc.format = render::Format_R8G8B8A8_UNorm;
		specularCubeDesc.layers = 6;
		specularCubeDesc.mipLevels = render::utils::ComputeMipLevels(specularCubeDesc.extent.width, specularCubeDesc.extent.height);
		check(specularCubeDesc.mipLevels <= render::utils::ComputeMipLevels(specularCubeDesc.extent.width, specularCubeDesc.extent.height));
		specularCubeDesc.dimension = render::ImageDimension_Cube;
		specularCubeDesc.isShaderResource = true;
		specularCubeDesc.debugName = "specular_cubemap";
		render::TextureHandle specularCubemap = device->CreateTexture(specularCubeDesc);

		render::TextureDescription brdfTexDesc;
		brdfTexDesc.extent = {m_irradianceResources.rt->m_info.extent.width, m_irradianceResources.rt->m_info.extent.height, 1};
		brdfTexDesc.isShaderResource = true;
		brdfTexDesc.format = render::Format_R16G16_UNorm;
		render::TextureHandle brdf = device->CreateTexture(brdfTexDesc);

		// Load image from file
		render::TextureHandle hdrFileContent;
		check(rendersystem::textureloader::LoadHDRTextureFromFile(&hdrFileContent, device, info.hdrFilepath, true));

		// Prepare uniform data
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

		// Render calls
		renderSystem->BeginMarker("Preprocess HDR cubemap");

		// Equirectangular pass
		renderSystem->BeginMarker("Equirectangular");
		CameraData cd;
		cd.Projection = captureProjection;
		for (uint32_t i = 0; i < 6; ++i)
		{
			cd.InvView = captureViews[i];
			//cd.InvView = glm::inverse(captureViews[i]);
			cd.ViewProjection = captureProjection * captureViews[i];
			m_irradianceResources.PrepareDraw(renderSystem, m_irradianceResources.rt->m_info.extent, m_irradianceResources.equirectangularShader);
			renderSystem->SetTextureSlot("u_map", hdrFileContent);
			renderSystem->SetShaderProperty("u_camera", &cd, sizeof(cd));
			m_irradianceResources.DrawCube(renderSystem);
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
		rendersystem::GenerateMipMaps(renderSystem->GetCommandList(), cubemap);
		renderSystem->SetTextureAsResourceBinding(cubemap);
		renderSystem->EndMarker();

		// Irradiance pass
		renderSystem->BeginMarker("Irradiance");
		for (uint32_t i = 0; i < 6; ++i)
		{
			cd.InvView = captureViews[i];
			//cd.InvView = glm::inverse(captureViews[i]);
			cd.ViewProjection = captureProjection * captureViews[i];
			m_irradianceResources.PrepareDraw(renderSystem, { irradianceCubemap->m_description.extent.width, irradianceCubemap->m_description.extent.height }, m_irradianceResources.irradianceShader);
			renderSystem->SetTextureSlot("u_cubemap", cubemap);
			renderSystem->SetShaderProperty("u_camera", &cd, sizeof(cd));
			m_irradianceResources.DrawCube(renderSystem);
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

		// Specular pass
		renderSystem->BeginMarker("Specular");
		uint32_t mipLevels = specularCubeDesc.mipLevels;
		for (uint32_t mip = 0; mip < mipLevels; ++mip)
		{
			uint32_t mipWidth = specularCubeDesc.extent.width >> mip;
			uint32_t mipHeight = specularCubeDesc.extent.height >> mip;
			float roughness = (float)mip / (float)(mipLevels - 1);
			glm::vec4 r = { roughness, roughness, roughness, roughness };
			for (uint32_t i = 0; i < 6; ++i)
			{
				cd.InvView = captureViews[i];
				//cd.InvView = glm::inverse(captureViews[i]);
				cd.ViewProjection = captureProjection * captureViews[i];
				m_irradianceResources.PrepareDraw(renderSystem, { mipWidth, mipHeight }, m_irradianceResources.specularShader);
				renderSystem->SetTextureSlot("u_cubemap", cubemap);
				renderSystem->SetShaderProperty("u_camera", &cd, sizeof(cd));
				renderSystem->SetShaderProperty("u_data", &r, sizeof(r));
				m_irradianceResources.DrawCube(renderSystem);
				renderSystem->ClearState();

				render::CopyTextureInfo info;
				info.extent = {mipWidth, mipHeight, 1};
				info.srcOffset = {};
				info.dstOffset = {};
				info.srcLayer = { 0, 0, 1 };
				info.dstLayer = { mip, i, 1 };
				renderSystem->CopyTextureToTexture(m_irradianceResources.rt->m_description.colorAttachments[0].texture, specularCubemap, &info, 1);
				renderSystem->SetTextureLayout(specularCubemap, render::ImageLayout_ShaderReadOnly, render::TextureSubresourceRange(mip, 1, i, 1));
			}
		}
		renderSystem->EndMarker();

		// BRDF
		renderSystem->BeginMarker("BRDF");
		renderSystem->ClearState();
		renderSystem->SetRenderTarget(m_irradianceResources.rt);
		renderSystem->SetShader(m_irradianceResources.brdfShader);
		renderSystem->SetViewport(m_irradianceResources.rt->m_info.GetViewport());
		renderSystem->SetScissor(m_irradianceResources.rt->m_info.GetScissor());
		renderSystem->DrawFullscreenQuad();
		renderSystem->ClearState();
		render::CopyTextureInfo copyInfo;
		copyInfo.extent = { brdf->m_description.extent.width, brdf->m_description.extent.height, 1 };
		copyInfo.srcOffset = {};
		copyInfo.dstOffset = {};
		copyInfo.srcLayer = { 0, 0, 1 };
		copyInfo.dstLayer = { 0, 0, 1 };
		renderSystem->CopyTextureToTexture(m_irradianceResources.rt->m_description.colorAttachments[0].texture, brdf, &copyInfo, 1);
		renderSystem->SetTextureLayout(specularCubemap, render::ImageLayout_ShaderReadOnly, render::TextureSubresourceRange(0, 1, 0, 1));
		renderSystem->EndMarker();

		renderSystem->EndMarker();

		PreprocessIrradianceResult res;
		res.cubemap = cubemap;
		res.irradianceCubemap = irradianceCubemap;
		res.specularCubemap = specularCubemap;
		res.brdf = brdf;
		return res;
	}
}