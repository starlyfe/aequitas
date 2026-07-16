#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D uScene;
layout(set = 0, binding = 1) uniform sampler2D uAO;

layout(push_constant) uniform Push {
    vec4 params; // x = ao strength (0..1)
} pc;

void main() {
    vec3 color = texture(uScene, inUV).rgb;
    float ao = texture(uAO, inUV).r;
    // Keep sky (bright/unoccluded) mostly untouched; soften contact shadows only.
    float factor = mix(1.0, ao, pc.params.x);
    outColor = vec4(color * factor, 1.0);
}
