#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out float outAO;

layout(set = 0, binding = 0) uniform sampler2D uAO;

layout(push_constant) uniform Push {
    vec4 dirTexel; // xy = blur direction in UV, zw unused
} pc;

void main() {
    // 5-tap separable box blur.
    vec2 dir = pc.dirTexel.xy;
    float c = texture(uAO, inUV).r;
    float a = texture(uAO, inUV - dir * 2.0).r;
    float b = texture(uAO, inUV - dir).r;
    float d = texture(uAO, inUV + dir).r;
    float e = texture(uAO, inUV + dir * 2.0).r;
    outAO = (a + b + c + d + e) * 0.2;
}
