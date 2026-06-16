

struct Model
{
    mat4 worldTransform;
};

#ifndef _MODEL_NOT_DEFINE_UNIFORMS_

#ifndef _MODEL_SET_
#define _MODEL_SET_ 1
#endif

#ifndef _MODEL_AS_UNIFORM_

layout (set = _MODEL_SET_, binding = 0) readonly buffer ModelBlockBuffer
{
    Model data[];
} u_modelBuffer;

#ifndef _MODEL_NOT_PREV_FRAME_
layout (set = _MODEL_SET_, binding = 1) readonly buffer PrevModelBlockBuffer
{
    Model data[];
} u_prevModelBuffer;
#endif

#else

layout (set = _MODEL_SET_, binding = 0) uniform ModelBlockBuffer
{
    Model data;
} u_modelBuffer;

#ifndef _MODEL_NOT_PREV_FRAME_
layout (set = _MODEL_SET_, binding = 1) uniform PrevModelBlockBuffer
{
    Model data;
} u_prevModelBuffer;
#endif

#endif

#endif
