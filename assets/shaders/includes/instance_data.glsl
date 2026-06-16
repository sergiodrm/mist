
struct InstanceID
{
    int modelId;
    int materialId;
    ivec2 _padding;
};


layout(push_constant) uniform InstanceBlock
{
	InstanceID data;
} u_instance;