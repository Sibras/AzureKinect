#version 430 core

layout(binding = 2) uniform InvResolution {
    vec2 invResolution;
};
layout(binding = 1) uniform sampler2D depthTexture;

out vec3 fragOutput;

void main()
{
    // Get UV coordinates
    vec2 cUV = gl_FragCoord.xy * invResolution;

    // Mirror the image
    cUV = 1.0f - cUV;

    // Load value from texture
    float depthVal = texture(depthTexture, cUV).r;

    // Convert to visible range
    //  K4A_DEPTH_MODE_NFOV_2X2BINNED = 500 -> 5800
    //  K4A_DEPTH_MODE_NFOV_UNBINNED = 500 -> 4000
    //  K4A_DEPTH_MODE_WFOV_2X2BINNED = 250 -> 3000
    //  K4A_DEPTH_MODE_WFOV_UNBINNED = 250 -> 2500
    // with a uint16 then max = 35536 which has been converted to 0->1 range
    const float minRange = 500.0f / 65536.0f;
    const float maxRange = 4000.0f / 65536.0f;
    const float scale = 1.0f / (maxRange - minRange);
    fragOutput = vec3((depthVal - minRange) * scale);
}
