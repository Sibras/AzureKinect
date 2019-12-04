#version 430 core

layout(location = 0) in vec3 normal;

out vec4 fragOutput;

void main()
{
    vec3 colour = vec3(1.0f, 1.0f, 0.0f) * normalize(normal).z;
    fragOutput = vec4(colour, 1.0f);
}
