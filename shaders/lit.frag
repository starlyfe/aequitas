#version 450

layout(location = 0) in vec3 inColor;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in float inGlow;

layout(push_constant) uniform PushConstants {
    mat4 viewProj;
    vec3 sunDir;
    float ambient;
} pc;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 n = normalize(inNormal);
    float ndotl = max(dot(n, -normalize(pc.sunDir)), 0.0);
    float lit = pc.ambient + (1.0 - pc.ambient) * ndotl;

    vec3 shaded = inColor * lit;
    shaded += inColor * inGlow * 0.75;

    outColor = vec4(shaded, 1.0);
}
