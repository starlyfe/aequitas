#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;

layout(location = 3) in vec3 inInstPos;
layout(location = 4) in float inInstYRot;
layout(location = 5) in float inInstScale;
layout(location = 6) in vec4 inInstColor;

layout(push_constant) uniform PushConstants {
    mat4 viewProj;
    vec3 sunDir;
    float ambient;
} pc;

layout(location = 0) out vec3 outColor;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out float outGlow;

void main() {
    float c = cos(inInstYRot);
    float s = sin(inInstYRot);
    mat3 rot = mat3(
        c, 0.0, -s,
        0.0, 1.0, 0.0,
        s, 0.0, c
    );

    vec3 worldPos = rot * (inPos * inInstScale) + inInstPos;
    vec3 worldNormal = normalize(rot * inNormal);

    gl_Position = pc.viewProj * vec4(worldPos, 1.0);

    outColor = inColor * inInstColor.rgb;
    outNormal = worldNormal;
    outGlow = inInstColor.a;
}
