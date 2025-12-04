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

__device__ inline const rtf::InstanceTransform* selectTransform(const rtf::IntersectionInfo& info, float time) {
    const rtf::InstanceTransform* transforms = info.transforms;
    if (transforms == nullptr || info.transformCount == 0) {
        return nullptr;
    }

    uint32_t selectedIndex = 0u;
    for (uint32_t idx = 0u; idx < info.transformCount; ++idx) {
        if (transforms[idx].time <= time) {
            selectedIndex = idx;
        } else {
            break;
        }
    }
    return transforms + selectedIndex;
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

    constexpr uint32_t samples = 32u;
    float3 color = { 0.0f, 0.0f, 0.0f };
    for ( uint32_t i = 0; i < samples; ++i )
    {
        const float time = i / static_cast<float>(samples);
        // jitter: time = (i + rand_f32(seed)) / samples
        hiprtSceneTraversalClosest tr(scene, ray, hiprtFullRayMask, hiprtTraversalHintDefault, nullptr, nullptr, 0, time);
        hiprtHit hit = tr.getNextHit();
        if (hit.hasHit()) {
            float3 sampleColor = { 0.0f, 0.0f, 0.0f };
            const rtf::IntersectionInfo& info = intersectionInfoBuffer[hit.instanceID];
            const rtf::MeshInfo* mesh = info.mesh;
            const rtf::InstanceTransform* transform = selectTransform(info, time);
            if (mesh != nullptr && mesh->vertices != nullptr && mesh->vertexIds != nullptr && hit.primID < mesh->triangleCount) {
                const uint3 vertexIds = mesh->vertexIds[hit.primID];
                float3 v0 = mesh->vertices[vertexIds.x];
                float3 v1 = mesh->vertices[vertexIds.y];
                float3 v2 = mesh->vertices[vertexIds.z];

                if (transform != nullptr) {
                    v0 = applyTransform(*transform, v0);
                    v1 = applyTransform(*transform, v1);
                    v2 = applyTransform(*transform, v2);
                }

                const float3 edge0 = { v1.x - v0.x, v1.y - v0.y, v1.z - v0.z };
                const float3 edge1 = { v2.x - v0.x, v2.y - v0.y, v2.z - v0.z };
                float3 rawNormal = {
                    edge0.y * edge1.z - edge0.z * edge1.y,
                    edge0.z * edge1.x - edge0.x * edge1.z,
                    edge0.x * edge1.y - edge0.y * edge1.x
                };

                const float lenSq = rawNormal.x * rawNormal.x + rawNormal.y * rawNormal.y + rawNormal.z * rawNormal.z;
                if (lenSq > 0.0f) {
                    const float invLen = rsqrtf(lenSq);
                    rawNormal.x *= invLen;
                    rawNormal.y *= invLen;
                    rawNormal.z *= invLen;
                } else {
                    rawNormal = { 0.0f, 0.0f, 1.0f };
                }

                sampleColor.x = (rawNormal.x + 1.0f) * 0.5f;
                sampleColor.y = (rawNormal.y + 1.0f) * 0.5f;
                sampleColor.z = (rawNormal.z + 1.0f) * 0.5f;
            } else {
                uint32_t seed = hit.instanceID;
                sampleColor.x = rand_f32(seed);
                sampleColor.y = rand_f32(seed);
                sampleColor.z = rand_f32(seed);
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
    pixels[index * 4 + 3] = 1.0f;
}
