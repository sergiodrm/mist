
#include "RenderResource.h"

namespace Mist
{
	
	const char* Mist::GetRenderResourceTypeStr(eResourceType type)
	{
		switch (type)
		{
		case RenderResource_Mesh: return "RenderResource_Mesh";
		case RenderResource_Material:return "RenderResource_Material";
		case RenderResource_Texture: return "RenderResource_Texture";
		case RenderResource_Model: return "RenderResource_Model";
		case RenderResource_RenderTarget: return "RenderResource_RenderTarget";
		}
		check(false);
		return nullptr;
	}

}
