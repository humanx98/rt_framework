#include <hip/amd_detail/amd_hip_vector_types.h>
#include <hip/hip_runtime.h>
#include <hiprt/hiprt_device.h>
#include "rand.hip.h"
#include "camera.hip.h"
#include "../HiprtIntersectionInfo.h"

__device__ inline float3 applyTransform(const rtf::InstanceTransform& transform, const float3& point) {
    const float4 coord = { point.x, point.y, point.z, 1.0f };
    const float4 row0 = transform.rows[0];
    const float4 row1 = transform.rows[1];
    const float4 row2 = transform.rows[2];

    float3 result;
    result.x = row0.x * coord.x + row0.y * coord.y + row0.z * coord.z + row0.w * coord.w;
    result.y = row1.x * coord.x + row1.y * coord.y + row1.z * coord.z + row1.w * coord.w;
    result.z = row2.x * coord.x + row2.y * coord.y + row2.z * coord.z + row2.w * coord.w;
    return result;
}

struct TransformSampleResult {
    rtf::InstanceTransform transform;
    bool valid{false};
};

__device__ inline float clampFloat(float value, float minValue, float maxValue) {
    return fminf(fmaxf(value, minValue), maxValue);
}

__device__ inline float4 lerpFloat4(const float4& a, const float4& b, float t) {
    float4 result;
    result.x = a.x + (b.x - a.x) * t;
    result.y = a.y + (b.y - a.y) * t;
    result.z = a.z + (b.z - a.z) * t;
    result.w = a.w + (b.w - a.w) * t;
    return result;
}

__device__ inline TransformSampleResult sampleTransform(const rtf::IntersectionInfo& info, float time) {
    TransformSampleResult result;
    const rtf::InstanceTransform* transforms = info.transforms;
    const uint32_t transformCount = info.transformCount;
    if (transforms == nullptr || transformCount == 0u) {
        return result;
    }

    if (transformCount == 1u) {
        result.transform = transforms[0];
        result.valid = true;
        return result;
    }

    uint32_t upperIndex = transformCount - 1u;
    for (uint32_t idx = 0u; idx < transformCount - 1u; ++idx) {
        if (time <= transforms[idx + 1u].time) {
            upperIndex = idx + 1u;
            break;
        }
    }

    if (upperIndex == 0u) {
        result.transform = transforms[0];
        result.valid = true;
        return result;
    }

    const uint32_t lowerIndex = upperIndex - 1u;
    const rtf::InstanceTransform& lower = transforms[lowerIndex];
    const rtf::InstanceTransform& upper = transforms[upperIndex];
    const float timeSpan = upper.time - lower.time;
    float factor = 0.0f;
    if (timeSpan > 0.0f) {
        factor = clampFloat((time - lower.time) / timeSpan, 0.0f, 1.0f);
    }

    rtf::InstanceTransform interpolated;
    interpolated.rows[0] = lerpFloat4(lower.rows[0], upper.rows[0], factor);
    interpolated.rows[1] = lerpFloat4(lower.rows[1], upper.rows[1], factor);
    interpolated.rows[2] = lerpFloat4(lower.rows[2], upper.rows[2], factor);
    interpolated.rows[3] = lerpFloat4(lower.rows[3], upper.rows[3], factor);
    interpolated.time = time;

    result.transform = interpolated;
    result.valid = true;
    return result;
}

__device__ inline float3 subtractVec(const float3& a, const float3& b) {
    return { a.x - b.x, a.y - b.y, a.z - b.z };
}

__device__ inline float3 crossVec(const float3& a, const float3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

__device__ inline float dotVec(const float3& a, const float3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

__device__ inline float3 normalizeOrDefault(const float3& v, const float3& fallback) {
    const float lenSq = dotVec(v, v);
    if (lenSq > 0.0f) {
        const float invLen = rsqrtf(lenSq);
        return { v.x * invLen, v.y * invLen, v.z * invLen };
    }
    return fallback;
}

__device__ inline float3 computeIntersectionPoint(const hiprtRay& ray, float t) {
    return {
        ray.origin.x + ray.direction.x * t,
        ray.origin.y + ray.direction.y * t,
        ray.origin.z + ray.direction.z * t
    };
}

__device__ inline float computePositionCorrection(const float3& triPos0, const float3& triPos1, const float3& triPos2, const float3& intersectionPoint) {
    const float3 e1 = subtractVec(triPos1, triPos0);
    const float3 e2 = subtractVec(triPos2, triPos0);
    const float3 normal = normalizeOrDefault(crossVec(e2, e1), { 0.0f, 0.0f, 1.0f });
    const float d = -dotVec(triPos0, normal);
    return -(dotVec(intersectionPoint, normal) + d);
}

HIPRT_DEVICE bool intersectFunc(uint32_t geomType, uint32_t rayType, const hiprtFuncTableHeader& tableHeader, const hiprtRay& ray, void* payload, hiprtHit& hit) {
    const uint32_t index = tableHeader.numGeomTypes * rayType + geomType;
    const void* data = tableHeader.funcDataSets[index].intersectFuncData;
    switch (index) {
    default:
        break;
    }

    return false;
}

HIPRT_DEVICE bool filterFunc(uint32_t geomType, uint32_t rayType, const hiprtFuncTableHeader& tableHeader, const hiprtRay& ray, void* payload, const hiprtHit& hit) {
    const uint32_t index = tableHeader.numGeomTypes * rayType + geomType;
    const void* data = tableHeader.funcDataSets[index].filterFuncData;
    switch (index) {
    default:
        break;
    }

    return false;
}

extern "C" __global__ void RenderNormals(hiprtScene scene, Camera camera, float* pixels, uint2 resolution, bool flip_y) {
    uint32_t index = blockIdx.x * blockDim.x + threadIdx.x;
    if (index >= resolution.x * resolution.y) {
        return;
    }

    const uint32_t y = index / resolution.x;
    const uint32_t x = index % resolution.x;

    // float3 color = make_float3(0.5f, 0.5f, 0.0f);
    hiprtRay ray = camera.generateRay(resolution, x, y);
    hiprtSceneTraversalClosest tr(scene, ray);
    hiprtHit hit = tr.getNextHit();

    float3 color = { 0.0f, 0.0f, 0.0f };
    if (hit.hasHit()) {
        float3 n = hiprt::normalize(hit.normal);
        color.x = ((n.x + 1.0f) * 0.5f);
        color.y = ((n.y + 1.0f) * 0.5f);
        color.z = ((n.z + 1.0f) * 0.5f);
    }

    if (flip_y) {
        uint32_t flipped_y = resolution.y - y - 1;
        index = resolution.x * flipped_y + x;
    }
    pixels[index * 4 + 0] = color.x;
    pixels[index * 4 + 1] = color.y;
    pixels[index * 4 + 2] = color.z;
    pixels[index * 4 + 3] = 1.0f;
}

extern "C" __global__ void RenderPrimitives(hiprtScene scene, Camera camera, float* pixels, uint2 resolution, bool flip_y) {
    uint32_t index = blockIdx.x * blockDim.x + threadIdx.x;
    if (index >= resolution.x * resolution.y) {
        return;
    }

    const uint32_t y = index / resolution.x;
    const uint32_t x = index % resolution.x;

    hiprtRay ray = camera.generateRay(resolution, x, y);
    hiprtSceneTraversalClosest tr(scene, ray);
    hiprtHit hit = tr.getNextHit();

    float3 color = { 0.0f, 0.0f, 0.0f };
    if (hit.hasHit()) {
        uint32_t seed = hit.primID;
        color.x = rand_f32(seed);
        color.y = rand_f32(seed);
        color.z = rand_f32(seed);
    }

    if (flip_y) {
        uint32_t flipped_y = resolution.y - y - 1;
        index = resolution.x * flipped_y + x;
    }
    pixels[index * 4 + 0] = color.x;
    pixels[index * 4 + 1] = color.y;
    pixels[index * 4 + 2] = color.z;
    pixels[index * 4 + 3] = 1.0f;
}

extern "C" __global__ void RenderMotionBlur(hiprtScene scene, Camera camera, float* pixels, uint2 resolution, bool flip_y, const rtf::IntersectionInfo* intersectionInfoBuffer) {
    uint32_t index = blockIdx.x * blockDim.x + threadIdx.x;
    if (index >= resolution.x * resolution.y) {
        return;
    }

    const uint32_t y = index / resolution.x;
    const uint32_t x = index % resolution.x;
    hiprtRay ray = camera.generateRay(resolution, x, y);

    constexpr uint32_t samples = 4u; 
    float3 color = { 0.0f, 0.0f, 0.0f };
    float positionCorrectionSum = 0.0f;
    uint32_t positionCorrectionCount = 0u;
    for ( uint32_t i = 0; i < samples; ++i )
    {
        const float time = i / static_cast<float>(samples);
//        const float time = i / static_cast<float>(samples);
        // jitter: time = (i + rand_f32(seed)) / samples
        hiprtSceneTraversalClosest tr(scene, ray, hiprtFullRayMask, hiprtTraversalHintDefault, nullptr, nullptr, 0, time);
        hiprtHit hit = tr.getNextHit();
        if (hit.hasHit()) {
            float3 sampleColor = { 0.0f, 0.0f, 0.0f };
            float positionCorrection = 0.0f;
            bool positionCorrectionValid = false;

            const bool hasInstanceInfo =
                (intersectionInfoBuffer != nullptr) && (hit.instanceID != hiprtInvalidValue);
            if (hasInstanceInfo) {
                const rtf::IntersectionInfo& info = intersectionInfoBuffer[hit.instanceID];
                const rtf::MeshInfo* mesh = info.mesh;
                if (mesh != nullptr && mesh->vertices != nullptr && mesh->vertexIds != nullptr &&
                    hit.primID < mesh->triangleCount) {
                    const uint3 vertexIds = mesh->vertexIds[hit.primID];
                    float3 v0 = mesh->vertices[vertexIds.x];
                    float3 v1 = mesh->vertices[vertexIds.y];
                    float3 v2 = mesh->vertices[vertexIds.z];

                    const TransformSampleResult transformSample = sampleTransform(info, time);
                    if (transformSample.valid) {
                        v0 = applyTransform(transformSample.transform, v0);
                        v1 = applyTransform(transformSample.transform, v1);
                        v2 = applyTransform(transformSample.transform, v2);
                    }

                    float3 rawNormal = crossVec(subtractVec(v2, v0), subtractVec(v1, v0));
                    rawNormal = normalizeOrDefault(rawNormal, { 0.0f, 0.0f, 1.0f });

                    sampleColor.x = (rawNormal.x + 1.0f) * 0.5f;
                    sampleColor.y = (rawNormal.y + 1.0f) * 0.5f;
                    sampleColor.z = (rawNormal.z + 1.0f) * 0.5f;

                    const float3 worldIntersection = computeIntersectionPoint(ray, hit.t);
                    positionCorrection = computePositionCorrection(v0, v1, v2, worldIntersection);
                    positionCorrectionValid = true;
                }
            }

            if (!positionCorrectionValid) {
                uint32_t seed = hit.instanceID;
                sampleColor.x = rand_f32(seed);
                sampleColor.y = rand_f32(seed);
                sampleColor.z = rand_f32(seed);
            } else {
                positionCorrectionSum += positionCorrection;
                positionCorrectionCount += 1u;
            }

            color.x += sampleColor.x;
            color.y += sampleColor.y;
            color.z += sampleColor.z;
        }
    }
    color = color / samples;

    if (flip_y) {
        uint32_t flipped_y = resolution.y - y - 1;
        index = resolution.x * flipped_y + x;
    }
    pixels[index * 4 + 0] = color.x;
    pixels[index * 4 + 1] = color.y;
    pixels[index * 4 + 2] = color.z;
    float avgPositionCorrection = positionCorrectionCount > 0u
        ? (1.0f - (positionCorrectionSum / static_cast<float>(positionCorrectionCount)))
        : 0.0f;
    pixels[index * 4 + 3] = 1.0f;
}
