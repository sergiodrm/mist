#version 460


layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;

// Vertex output
layout (location = 0) in vec3 gs_in_Normal[];
layout (location = 1) in vec2 gs_in_UV[];
layout (location = 2) in vec3 gs_in_Color[];
layout (location = 3) in vec3 gs_in_WorldPos[];
layout (location = 4) in vec3 gs_in_Tangent[];
layout (location = 5) in vec4 gs_in_CurrWSPos[];
layout (location = 6) in vec4 gs_in_PrevWSPos[];
layout (location = 7) in mat3 gs_in_TBN[];

// Geometry output
layout (location = 0) out vec3 gs_out_Normal;
layout (location = 1) out vec2 gs_out_UV;
layout (location = 2) out vec3 gs_out_Color;
layout (location = 3) out vec3 gs_out_WorldPos;
layout (location = 4) out vec3 gs_out_Tangent;
layout (location = 5) out vec4 gs_out_CurrWSPos;
layout (location = 6) out vec4 gs_out_PrevWSPos;
layout (location = 7) out mat3 gs_out_TBN;

vec3 GetNormal()
{
    vec3 a = vec3(gl_in[0].gl_Position) - vec3(gl_in[1].gl_Position);
    vec3 b = vec3(gl_in[2].gl_Position) - vec3(gl_in[1].gl_Position);
    return normalize(cross(a, b));
}

vec4 Explode(vec4 pos, vec3 n, float m)
{
    vec3 dir = n * m;
    return pos + vec4(dir, 0.f);
}

void main()
{
    vec4 n = vec4(GetNormal(),1);
    for (int i = 0; i < 3; ++i)
    {
        gl_Position = Explode(gl_in[i%3].gl_Position, n.xyz, 0.1f);
        gs_out_Normal = gs_in_Normal[i%3];
        gs_out_UV = gs_in_UV[i%3];
        gs_out_Color = gs_in_Color[i%3];
        gs_out_WorldPos = gs_in_WorldPos[i%3];
        gs_out_Tangent = gs_in_Tangent[i%3];
        gs_out_CurrWSPos = gs_in_CurrWSPos[i%3];
        gs_out_PrevWSPos = gs_in_PrevWSPos[i%3];
        gs_out_TBN = gs_in_TBN[i%3];
        EmitVertex();
    }
    EndPrimitive();
}