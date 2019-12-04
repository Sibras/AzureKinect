#version 430 core 

layout(binding = 0) uniform TransformData {
    vec2 c;
    vec2 f;
    vec2 k14;
    vec2 k25;
    vec2 k36;
    vec2 p;
};

layout(binding = 1) uniform CameraData {
    mat4 viewProjection;
};

layout(binding = 3) uniform ImageResolution {
    vec2 invResolution;
};

layout(location = 0) in vec3 vertexPos;
layout(location = 1) in vec3 normal;
layout(location = 2) in mat4 transform;

layout(location = 0) smooth out vec3 positionOut;
layout(location = 1) smooth out vec3 normalOut;

void main()
{
    // Transform to model space
    vec4 position = transform * vec4(vertexPos, 1.0f);

    // Transform position to image space using brown conrady
    vec2 pxy = vec2(position.x, position.y) / position.z;
    vec2 p2 = pxy * pxy;
    float xyp = pxy.x * pxy.y;
    float rs = p2.x + p2.y;
    float rss = rs * rs;
    float rsc = rss * rs;
    vec2 ab = 1.0f + k14 * rs + k25 * rss + k36 * rsc;
    float bi = (ab.y != 0.0f) ? 1.0f / ab.y : 1.0f;
    float d = ab.x * bi;
    vec2 p_d = pxy * d;
    vec2 rs_2p2 = rs + 2.0f * p2;
    p_d += rs_2p2 * p.yx + 2.0f * xyp * p;
    vec2 uv = p_d * f + c;

    // Scale image UV value into screen space
    uv = (uv * 2.0f) * invResolution;
    uv -= 1.0f;
    uv *= -1.0f;

    // Output position just using the approximated z    
    vec4 approx = viewProjection * position;
    gl_Position = vec4(uv, approx.z / approx.w, 1.0f);

    // Transform normal using using the approximated projection
    mat4 transformIT = transpose(inverse(transform));
    vec4 transNormal = transformIT * vec4(normal, 0.0f);
    normalOut = transNormal.xyz;
    positionOut = position.xyz;
}
