#version 460

layout(set = 0, binding = 0) uniform sampler2D inputColour;
layout(set = 0, binding = 1) uniform sampler2D u_ShadowTexture;

layout(push_constant) uniform PostFXPushConstant {
    int isEnableBloom;
    vec2 texelSize;
} pc;

layout(location = 0) in vec2 inFragTexcoord;
layout(location = 0) out vec4 outColour;

vec3 Tonemap_Reinhard(vec3 color) {
    return color / (color + vec3(1.0));
}

float brightness(vec3 color) {
    return dot(color, vec3(0.2126, 0.7152, 0.0722));
}

void main() {
    vec3 baseColor     = texture(inputColour, inFragTexcoord).rgb;
    float shadowFactor = texture(u_ShadowTexture, inFragTexcoord).r;

    // 1. Bloom Extract (only if enabled)
    vec3 bloom = vec3(0.0);
    if (pc.isEnableBloom != 0) {
        float kernel[3] = float[](0.25, 0.5, 0.25);
        for (int y = -3; y <= 3; ++y) {
            for (int x = -3; x <= 3; ++x) {
                vec2 offset = vec2(x, y) * pc.texelSize;
                vec3 bloomSample = texture(inputColour, inFragTexcoord + offset).rgb;
                float b = brightness(bloomSample);
                if (b > 1.0) {
                    int ax = abs(x);
                    int ay = abs(y);
                    bloom += bloomSample * kernel[ax] * kernel[ay];
                }
            }
        }
        bloom /= 36.0;
    }

    // 2. Shadow
    vec3 shadowed = baseColor * shadowFactor;

    // 3. ToneMapping
    vec3 toneMapped = Tonemap_Reinhard(shadowed);

    // 4. Apply Bloom if enabled
    vec3 result = toneMapped + bloom;

    outColour = vec4(result, 1.0);
}
