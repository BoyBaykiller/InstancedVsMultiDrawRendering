#version 460 core
#extension GL_KHR_shader_subgroup_basic : require
#extension GL_KHR_shader_subgroup_ballot : require

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
    uint SubgroupMaxActiveLanes;
    uint SubgroupSize;
    uint SubgroupCount;
    uint IsSubgroupUniform;
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
    const int indexInQuestion = UseDrawID ? gl_DrawID : gl_InstanceID;

    // Collect data
    {
        uint activeLanes = subgroupBallotBitCount(subgroupBallot(true));
        atomicMax(shaderInfoSSBO.Data.SubgroupMaxActiveLanes, activeLanes);
        
        shaderInfoSSBO.Data.SubgroupSize = gl_SubgroupSize;

        if (subgroupElect())
        {
            atomicAdd(shaderInfoSSBO.Data.SubgroupCount, 1u);    
        }

        if (indexInQuestion != subgroupBroadcastFirst(indexInQuestion))
        {
            shaderInfoSSBO.Data.IsSubgroupUniform = 0;
        }
    }

    const float triScale = 1.0 / sqrt(Count);

    vec2 translation;
    {
        float x = indexInQuestion * triScale;
        const float y = floor(x) * triScale;
        x = fract(x);

        translation = vec2(x, y);
    }

    const vec3 bary = vec3(gl_VertexID % 3 == 0, gl_VertexID % 3 == 1, gl_VertexID % 3 == 2);
    outVars.Color = bary;
    outVars.RecordedIndex = indexInQuestion;

    const Vertex vertex = Vertices[gl_VertexID % 3];
    const vec2 vertexPos = vertex.Position * triScale;
    gl_Position = vec4((translation + vertexPos) * 2.0 - 1.0, 0.0, 1.0);
}