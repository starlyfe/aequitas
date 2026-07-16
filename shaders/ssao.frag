#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out float outAO;

layout(set = 0, binding = 0) uniform sampler2D uDepth;

layout(push_constant) uniform Push {
    mat4 viewProj;
    mat4 invViewProj;
    vec4 params; // x=radius, y=bias, z=intensity, w=unused
} pc;

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

vec3 reconstruct_world(vec2 uv, float depth) {
    vec4 clip = vec4(uv.x * 2.0 - 1.0, uv.y * 2.0 - 1.0, depth, 1.0);
    vec4 world = pc.invViewProj * clip;
    return world.xyz / max(world.w, 1e-6);
}

vec3 project_uv_depth(vec3 worldPos, out float depth) {
    vec4 clip = pc.viewProj * vec4(worldPos, 1.0);
    vec3 ndc = clip.xyz / max(clip.w, 1e-6);
    depth = ndc.z;
    return vec3(ndc.x * 0.5 + 0.5, ndc.y * 0.5 + 0.5, depth);
}

void main() {
    float depth = texture(uDepth, inUV).r;
    if (depth >= 0.9995) {
        outAO = 1.0;
        return;
    }

    vec3 origin = reconstruct_world(inUV, depth);
    vec2 texel = 1.0 / vec2(textureSize(uDepth, 0));

    float dR = texture(uDepth, inUV + vec2(texel.x, 0.0)).r;
    float dU = texture(uDepth, inUV + vec2(0.0, texel.y)).r;
    vec3 pR = reconstruct_world(inUV + vec2(texel.x, 0.0), dR);
    vec3 pU = reconstruct_world(inUV + vec2(0.0, texel.y), dU);
    vec3 normal = normalize(cross(pR - origin, pU - origin));
    if (any(isnan(normal))) {
        normal = vec3(0.0, 1.0, 0.0);
    }

    vec3 tangent = normalize(abs(normal.y) < 0.999
                                 ? cross(vec3(0.0, 1.0, 0.0), normal)
                                 : cross(vec3(1.0, 0.0, 0.0), normal));
    vec3 bitangent = cross(normal, tangent);

    const int kSamples = 16;
    float radius = pc.params.x;
    float bias = pc.params.y;
    float occluded = 0.0;
    float seed = hash(inUV * 97.0);

    for (int i = 0; i < kSamples; ++i) {
        float fi = float(i);
        float u = hash(vec2(fi + 1.7, seed));
        float v = hash(vec2(seed + 3.1, fi + 0.3));
        float theta = 6.2831853 * u;
        float rz = v; // [0,1] along normal hemisphere
        float r = sqrt(max(1.0 - rz * rz, 0.0));
        vec3 hemi = vec3(r * cos(theta), rz, r * sin(theta));
        vec3 dir = normalize(tangent * hemi.x + normal * hemi.y + bitangent * hemi.z);
        float dist = radius * (0.15 + 0.85 * hash(vec2(fi, seed + 9.0)));
        vec3 samplePos = origin + dir * dist;

        float sampleDepth = 0.0;
        vec3 sampleUV = project_uv_depth(samplePos, sampleDepth);
        if (sampleUV.x < 0.0 || sampleUV.x > 1.0 || sampleUV.y < 0.0 || sampleUV.y > 1.0) {
            continue;
        }

        float sceneDepth = texture(uDepth, sampleUV.xy).r;
        float rangeCheck = smoothstep(0.0, 1.0, radius / max(length(origin - reconstruct_world(sampleUV.xy, sceneDepth)), 1e-3));
        if (sceneDepth + bias < sampleDepth) {
            occluded += rangeCheck;
        }
    }

    float ao = 1.0 - (occluded / float(kSamples)) * pc.params.z;
    outAO = clamp(ao, 0.0, 1.0);
}
