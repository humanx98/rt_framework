#include <hip/amd_detail/amd_hip_vector_types.h>
#include <hip/hip_runtime.h>
#include <hiprt/hiprt_device.h>
#include "rand.hip.h"
#include "camera.hip.h"

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

extern "C" __global__ void RenderMotionBlur(hiprtScene scene, Camera camera, float* pixels, uint2 resolution, bool flip_y) {
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
            uint32_t seed = hit.instanceID;
            color.x += rand_f32(seed);
            color.y += rand_f32(seed);
            color.z += rand_f32(seed);
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
