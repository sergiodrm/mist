
#ifndef _VIEW_PROJ_JITT_MATRIX_
#define _VIEW_PROJ_JITT_MATRIX_ (u_camera.data.jitteredViewProjection)
#endif

#ifndef _VIEW_PROJ_MATRIX_
#define _VIEW_PROJ_MATRIX_ (u_camera.data.viewProjection)
#endif

#ifndef _VERTEX_MESH_DISABLE_DEFAULT_VERTEX_
// Vertex input
layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec3 inColor;
layout (location = 3) in vec4 inTangent;
layout (location = 4) in vec2 inUV0;
layout (location = 5) in vec2 inUV1;
#endif // !_VERTEX_MESH_DISABLE_DEFAULT_VERTEX_

