#version 430 core

layout(location = 0) in vec3 positionIn;
layout(location = 1) in vec3 normalIn;
layout(location = 2) in float confidenceIn;

out vec4 fragOutput;

void main()
{
    // Normalise the inputs
    vec3 vormal = normalize(normalIn);
    vec3 viewDirection = normalize(positionIn); //camera is at 0,0,0

    // Just use basic dot3 shading
    float shade = dot(viewDirection, vormal);
    vec3 colour = vec3(1.0f, confidenceIn, 0.0f) * shade * shade; //heighten shading using dot3 squared
    fragOutput = vec4(colour, 0.6f);
}
