#version 430 core

layout(binding = 2) uniform InvResolution {
    vec2 invResolution;
    vec2 windowsOffset;
};
layout(binding = 2) uniform sampler2D colourTexture;

out vec3 fragOutput;

void main()
{
    // Get UV coordinates
    vec2 cUV = (gl_FragCoord.xy - windowsOffset) * invResolution;

    // Mirror the image
    cUV = 1.0f - cUV;

    // Load value from texture
    fragOutput = texture(colourTexture, cUV).rgb;
}
