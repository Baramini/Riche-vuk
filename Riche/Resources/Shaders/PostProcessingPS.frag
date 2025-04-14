#version 460
#extension GL_EXT_samplerless_texture_functions : enable

layout(set = 0, binding = 0) uniform sampler linearWrapSS;

layout(set = 1, binding = 0) uniform texture2D inputColour;
layout(set = 1, binding = 1) uniform texture2D u_ShadowTexture;
layout(set = 1, binding = 2) uniform texture2D u_BloomTexture;

layout(location = 0) in vec2 outUV;
layout(location = 0) out vec4 outColor;

void main() {
    vec3 baseColor = texture(sampler2D(inputColour, linearWrapSS), outUV).rgb;
    vec3 bloom = texture(sampler2D(u_BloomTexture, linearWrapSS), outUV).rgb;
    float shadow = texture(sampler2D(u_ShadowTexture, linearWrapSS), outUV).r;

    vec3 lit = baseColor * shadow;

    vec3 finalColor = lit + bloom;

    outColor = vec4(finalColor, 1.0);
}
