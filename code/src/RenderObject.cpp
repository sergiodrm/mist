#include "RenderObject.h"

#include <glm/gtx/transform.hpp>

namespace vkmmc
{
	glm::mat4 CalculateTransform(const RenderObjectTransform& objectTransform)
	{
		glm::mat4 t = glm::translate(glm::mat4{ 1.f }, objectTransform.Position);
		glm::mat4 r = glm::rotate(glm::mat4{ 1.f }, objectTransform.Rotation.x, { 1.f, 0.f, 0.f });
		r = glm::rotate(r, objectTransform.Rotation.y, { 0.f, 1.f, 0.f });
		r = glm::rotate(r, objectTransform.Rotation.z, { 0.f, 0.f, 1.f });
		glm::mat4 s = glm::scale(objectTransform.Scale);
		return t * r * s;
	}
}