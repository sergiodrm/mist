#include "RenderTypes.h"
#include "Vertex.h"


namespace vkmmc
{
	VertexInputDescription GetDefaultVertexDescription()
	{
		static bool built = false;
		static VertexInputDescription description = {};

		if (!built)
		{
			built = true;

			// One vertex buffer binding, with a pervertex rate
			VkVertexInputBindingDescription mainBinding = {};
			mainBinding.binding = 0;
			mainBinding.stride = sizeof(Vertex);
			mainBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

			description.Bindings.push_back(mainBinding);

			// Position stored at Location = 0
			VkVertexInputAttributeDescription positionAttribute = {};
			positionAttribute.binding = 0;
			positionAttribute.location = 0;
			positionAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
			positionAttribute.offset = offsetof(Vertex, Position);

			description.Attributes.push_back(positionAttribute);
		}
		return description;
	}
}