#version 430 core

layout(binding = 2) uniform InvResolution {
    vec2 invResolution;
};
layout(binding = 4) uniform sampler2D shadowTexture;

out vec4 fragOutput;

void main()
{
    // Get UV coordinates
    vec2 cUV = gl_FragCoord.xy * invResolution;

    // Mirror the image
    cUV = 1.0f - cUV;

    // Load value from texture
    float shadowVal = texture(shadowTexture, cUV).r;

    // Convert to visible range
    //  A value of 0xFF means visible else it is not (0)
    if (shadowVal <= 0.5f) {
        discard;
    }
    fragOutput = vec4(0.0f, 1.0f, 0.0f, 0.15f);
}
