#include "Scene.h"
#include "GenericUtils.h"


namespace Mist
{
	void TransformComponentToMatrix(const TransformComponent* transforms, glm::mat4* matrices, uint32_t count)
	{
		for (uint32_t i = 0; i < count; ++i)
			matrices[i] = math::ToMat4(transforms[i].Position, transforms[i].Rotation, transforms[i].Scale);
	}
}
