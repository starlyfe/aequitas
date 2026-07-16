#version 450

layout(location = 0) in vec3 inColor;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in float inGlow;

layout(push_constant) uniform PushConstants {
    mat4 viewProj;
    vec3 sunDir;
    float ambient;
    float sunIntensity;
} pc;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 n = normalize(inNormal);
    vec3 l = -normalize(pc.sunDir);

    // Soft wrap lighting + classic Lambert mix — less harsh low-poly shadow edges.
    float ndotl = max(dot(n, l), 0.0);
    float wrap = max(dot(n, l) * 0.5 + 0.5, 0.0);
    float diffuse = mix(ndotl, wrap * wrap, 0.45);

    float lit = pc.ambient + pc.sunIntensity * (1.0 - pc.ambient) * diffuse;
    // Gentle sky fill so undersides aren't pure black.
    float hemi = max(n.y * 0.5 + 0.5, 0.0);
    lit += 0.06 * hemi;

    vec3 shaded = inColor * lit;
    shaded += inColor * inGlow * 0.65;

    outColor = vec4(shaded, 1.0);
}
