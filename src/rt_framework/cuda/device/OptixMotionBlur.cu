#include <optix.h>
#include <optix_device.h>

#include "../OptixLaunchParams.h"

static __forceinline__ __device__ float3 cross3(const float3& a, const float3& b) {
    return make_float3(
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    );
}

static __forceinline__ __device__ float3 normalize3(const float3& v) {
    const float len2 = v.x * v.x + v.y * v.y + v.z * v.z;
    const float invLen = rsqrtf(len2 + 1e-20f);
    return make_float3(v.x * invLen, v.y * invLen, v.z * invLen);
}

static __forceinline__ __device__ float3 add3(const float3& a, const float3& b) {
    return make_float3(a.x + b.x, a.y + b.y, a.z + b.z);
}

static __forceinline__ __device__ float3 mul3(float s, const float3& v) {
    return make_float3(v.x * s, v.y * s, v.z * s);
}

static __forceinline__ __device__ float3 madd3(const float3& a, float s, const float3& b) {
    return make_float3(a.x + s * b.x, a.y + s * b.y, a.z + s * b.z);
}

static __forceinline__ __device__ uint32_t pcg32(uint32_t& state) {
    uint32_t oldState = state + 747796405u + 2891336453u;
    uint32_t word = ((oldState >> ((oldState >> 28) + 4)) ^ oldState) * 277803737u;
    state = (word >> 22) ^ word;
    return state;
}

static __forceinline__ __device__ float rand_float(uint32_t& state) {
    return static_cast<float>(pcg32(state)) * (1.0f / 4294967296.0f);
}

struct Ray {
    float3 origin;
    float3 direction;
};

static __forceinline__ __device__ Ray generateRay(const OptixCameraData& camera, uint2 pixel, uint2 resolution) {
    const float3 forward = normalize3(make_float3(camera.lookAt.x - camera.lookFrom.x, camera.lookAt.y - camera.lookFrom.y, camera.lookAt.z - camera.lookFrom.z));
    const float3 right = normalize3(cross3(forward, camera.up));
    const float3 up = normalize3(cross3(right, forward));

    const float aspect = static_cast<float>(resolution.x) / static_cast<float>(resolution.y);
    const float px = (2.0f * ((static_cast<float>(pixel.x) + 0.5f) / static_cast<float>(resolution.x)) - 1.0f) * tanf(camera.vfov * 0.5f) * aspect;
    const float py = (1.0f - 2.0f * ((static_cast<float>(pixel.y) + 0.5f) / static_cast<float>(resolution.y))) * tanf(camera.vfov * 0.5f);

    Ray ray;
    ray.origin = camera.lookFrom;
    ray.direction = normalize3(add3(add3(forward, mul3(px, right)), mul3(py, up)));
    return ray;
}

static __forceinline__ __device__ void storePayload(const float3& color) {
    optixSetPayload_0(__float_as_uint(color.x));
    optixSetPayload_1(__float_as_uint(color.y));
    optixSetPayload_2(__float_as_uint(color.z));
}

static __forceinline__ __device__ float3 readPayload() {
    return make_float3(
        __uint_as_float(optixGetPayload_0()),
        __uint_as_float(optixGetPayload_1()),
        __uint_as_float(optixGetPayload_2())
    );
}

extern "C" {
    __constant__ OptixLaunchParams optixLaunchParams;
}

static __forceinline__ __device__ float3 trace(const Ray& ray, float time) {
    unsigned int p0 = 0u;
    unsigned int p1 = 0u;
    unsigned int p2 = 0u;

    optixTrace(
        optixLaunchParams.scene,
        ray.origin,
        ray.direction,
        0.0f,
        1e16f,
        time,
        0xFF,
        OPTIX_RAY_FLAG_DISABLE_ANYHIT,
        0,
        1,
        0,
        p0, p1, p2
    );

    return make_float3(__uint_as_float(p0), __uint_as_float(p1), __uint_as_float(p2));
}

extern "C" __global__ void __raygen__motion_blur() {
    const uint3 idx = optixGetLaunchIndex();
    const uint3 dim = optixGetLaunchDimensions();

    if (idx.x >= dim.x || idx.y >= dim.y) {
        return;
    }

    const uint2 pixel = make_uint2(idx.x, idx.y);
    const uint2 resolution = make_uint2(optixLaunchParams.width, optixLaunchParams.height);

    Ray ray = generateRay(optixLaunchParams.camera, pixel, resolution);

    float3 color = make_float3(0.0f, 0.0f, 0.0f);
    uint32_t seed = pixel.y * resolution.x + pixel.x;
    const unsigned int samples = optixLaunchParams.samplesPerPixel > 0u
        ? optixLaunchParams.samplesPerPixel
        : 1u;

    for (unsigned int i = 0; i < samples; ++i) {
        const float jitter = rand_float(seed);
        const float time = (static_cast<float>(i) + jitter) / static_cast<float>(samples);
        color = add3(color, trace(ray, time));
    }

    color = mul3(1.0f / static_cast<float>(samples), color);

    unsigned int writeIndex = pixel.y * resolution.x + pixel.x;
    if (optixLaunchParams.flipY) {
        writeIndex = (optixLaunchParams.height - 1u - pixel.y) * resolution.x + pixel.x;
    }

    optixLaunchParams.colorBuffer[writeIndex] = make_float4(color.x, color.y, color.z, 1.0f);
}

extern "C" __global__ void __miss__constant() {
    storePayload(make_float3(0.0f, 0.0f, 0.0f));
}

extern "C" __global__ void __closesthit__motion() {
    uint32_t seed = optixGetInstanceId();
    const float3 color = make_float3(rand_float(seed), rand_float(seed), rand_float(seed));
    storePayload(color);
}
