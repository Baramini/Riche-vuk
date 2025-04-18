#version 460

layout(set = 0, binding = 0) uniform sampler2D inputColour;
layout(set = 0, binding = 1) uniform sampler2D u_ShadowTexture;

layout(push_constant) uniform PostFXPushConstant {
    int isEnableBloom;
} pc;

layout(location = 0) in vec2 inFragTexcoord;
layout(location = 0) out vec4 outColour;

vec3 Tonemap_Reinhard(vec3 color) {
    return color / (color + vec3(1.0));
}

void main() {
    // Shadow
    vec3 baseColor    = texture(inputColour,    inFragTexcoord).rgb;
    float shadowFactor= texture(u_ShadowTexture, inFragTexcoord).r;
    vec3 shadowed     = baseColor * shadowFactor;

    // ToneMapping
    vec3 toneMapped   = Tonemap_Reinhard(shadowed);

    // Bloom
    vec3 result = toneMapped;
    if (pc.isEnableBloom != 0) {
        float brightness = max(max(toneMapped.r, toneMapped.g), toneMapped.b);
        vec3 bloom       = brightness > 1.0 ? toneMapped * 0.25 : vec3(0.0);
        result += bloom;
    }

    outColour = vec4(result, 1.0);
}
