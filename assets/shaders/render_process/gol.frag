#version 460

struct Cell
{
    int state;
};

layout (location = 0) in vec2 inTexCoords;
layout (location = 0) out vec4 outColor;

layout (set = 0, binding = 0) uniform Params
{
    ivec2 cellsDimensions;
    vec2 resolution;
    float gridSize;
    vec3 padding;
} u_params;

layout (set = 1, binding = 0) buffer CellRead
{
    Cell data[];
} u_cells;

vec2 Gradient(vec2 v)
{
    const float b = -3.8;
    const float a = -b;
    const float c = 0.5;
    return a*v*v+b*v+c;
}

float GridGradient(vec2 coords, vec2 dims)
{
    vec2 f = fract(coords * dims);
    vec2 g = Gradient(f);
    return g.x+g.y;
}

int FromCoordsToIndex(int x, int y, ivec2 dims)
{
    return dims.x * y + x;
}

float SphereDistance(vec2 coords, float r)
{
    return length(coords - vec2(0.5,0.5)) - r;
}

void main()
{
    vec2 dims = vec2(u_params.cellsDimensions);
    vec2 gridCellSize = 1.f / dims;

    vec4 c = vec4(0.132,0.423,0.199,0);
#if 0
    float f = GridGradient(inTexCoords, dims);
#else
    vec2 d = fract(inTexCoords*dims);
    float f = SphereDistance(d, 0);
#endif

    ivec2 cellCoords = ivec2(floor(inTexCoords * vec2(u_params.cellsDimensions)));
    int index = FromCoordsToIndex(cellCoords.x, cellCoords.y, u_params.cellsDimensions);
    int state = u_cells.data[index].state;
    if (state == 1)
        c *= (1.0-f);
    else if (state == 2)
        c = vec4(1,0,0,0);
    else
        c *= f*0.05;
    //c *= f;
    outColor = c;
}
