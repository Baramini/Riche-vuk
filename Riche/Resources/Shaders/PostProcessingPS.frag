#version 460

layout(set = 0, binding = 0) uniform sampler linearWrapSS;

layout(set = 1, binding = 0) uniform texture2D inputColour;
layout(set = 1, binding = 1) uniform texture2D u_ShadowTexture;
layout(set = 1, binding = 2) uniform texture2D u_BloomTexture;

layout(push_constant) uniform PostFXPushConstant {
    int enableBloom;
} pc;

layout(location = 0) in vec2 inFragTexcoord;
layout(location = 0) out vec4 outColour;

vec3 Tonemap_Reinhard(vec3 color) {
    return color / (color + vec3(1.0));
}

void main() {
    vec3 hdr = texture(sampler2D(inputColour, linearWrapSS), inFragTexcoord).rgb;
    vec3 bloom = texture(sampler2D(u_BloomTexture, linearWrapSS), inFragTexcoord).rgb;
    float shadow = texture(sampler2D(u_ShadowTexture, linearWrapSS), inFragTexcoord).r;

    vec3 color = hdr + (pc.enableBloom != 0 ? bloom : vec3(0.0));
    color *= shadow;

    vec3 ldr = Tonemap_Reinhard(color);
    outColour = vec4(ldr, 1.0);
}
