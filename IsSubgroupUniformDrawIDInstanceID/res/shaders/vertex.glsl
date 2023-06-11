#version 460 core
#extension GL_KHR_shader_subgroup_basic : require

layout(location = 0) in vec3 Position;

struct Vertex
{
    vec2 Position;
};

const Vertex Vertices[] =
{
    Vertex(vec2(  0.0,   0.0 )),
    Vertex(vec2(  1.0,   0.0 )),
    Vertex(vec2(  0.5,   1.0 ))
};


struct ShaderInfo
{
    uint SubgroupSize;
    uint ScalarizationCount;
};

layout(binding = 0, std430) restrict buffer ShaderInfoSSBO
{
    ShaderInfo Data;
} shaderInfoSSBO;

layout(location = 0) out InOutVars
{
    vec3 Color;
    int RecordedIndex;
} outVars;

layout(location = 0) uniform bool UseDrawID;
layout(location = 1) uniform int Count;

void main()
{    
    const float TriScale = 1.0 / sqrt(Count);
    const int indexInQuestion = UseDrawID ? gl_DrawID : gl_InstanceID;

    vec2 proceduralOffset;
    {
        float x = indexInQuestion * TriScale;
        const float y = floor(x) * TriScale;
        x = fract(x);

        proceduralOffset = vec2(x, y);
    }

    if (subgroupElect())
    {
        atomicAdd(shaderInfoSSBO.Data.ScalarizationCount, 1u);
        shaderInfoSSBO.Data.SubgroupSize = gl_SubgroupSize;
    }

    const vec3 bary = vec3(gl_VertexID % 3 == 0, gl_VertexID % 3 == 1, gl_VertexID % 3 == 2);
    outVars.Color = bary;
    outVars.RecordedIndex = indexInQuestion;

    const Vertex vertex = Vertices[gl_VertexID];
    const vec2 vertexPos = vertex.Position * TriScale;
    gl_Position = vec4((proceduralOffset + vertexPos) * 2.0 - 1.0, 0.0, 1.0);
}