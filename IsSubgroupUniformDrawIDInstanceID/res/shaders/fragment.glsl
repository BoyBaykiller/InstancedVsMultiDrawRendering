#version 460 core

layout(location = 0) out vec4 FragColor;

layout(location = 0) in InOutVars
{
    vec3 Color;
    flat int RecordedIndex;
} inVars;

void main()
{
    FragColor = vec4(inVars.Color, 1.0);
}