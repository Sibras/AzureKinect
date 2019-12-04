#version 430 core

layout(binding = 2) uniform InvResolution {
    vec2 invResolution;
    vec2 windowsOffset;
};
layout(binding = 3) uniform sampler2D irTexture;

out vec3 fragOutput;

void main()
{
    // Get UV coordinates
    vec2 cUV = (gl_FragCoord.xy - windowsOffset) * invResolution;

    // Mirror the image
    cUV = 1.0f - cUV;

    // Load value from texture
    float depthVal = texture(irTexture, cUV).r;

    // Convert to visible range
    //  K4A_DEPTH_MODE_PASSIVE_IR = 0 -> 100
    // with a uint16 then max = 35536 which has been converted to 0->1 range
    const float minRange = 0.0f;
    const float maxRange = 1000.0f / 65536.0f;
    const float scale = 1.0f / (maxRange - minRange);
    fragOutput = vec3((depthVal - minRange) * scale);
}
