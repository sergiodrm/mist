#include "Scene/Scene.h"
#include "Utils/GenericUtils.h"
#include "Core/Debug.h"


namespace Mist
{
	const char* LightTypeToStr(ELightType e)
	{
		switch (e)
		{
		case ELightType::Point: return "Point";
		case ELightType::Directional: return "Directional";
		case ELightType::Spot: return "Spot";
		}
		check(false);
		return nullptr;
	}

	ELightType StrToLightType(const char* str)
	{
		if (!strcmp(str, "Point")) return ELightType::Point;
		if (!strcmp(str, "Spot")) return ELightType::Spot;
		if (!strcmp(str, "Directional")) return ELightType::Directional;
		check(false);
		return (ELightType)0xff;
	}

	void TransformComponentToMatrix(const TransformComponent* transforms, glm::mat4* matrices, uint32_t count)
	{
		for (uint32_t i = 0; i < count; ++i)
			matrices[i] = math::ToMat4(transforms[i].Position, transforms[i].Rotation, transforms[i].Scale);
	}
}
