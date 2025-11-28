#pragma once

#include <hiprt/hiprt_types.h>
#include <hiprt/hiprt_math.h>

struct Camera {
    hiprtFloat3 from;
    hiprtFloat3 target;
    hiprtFloat3 up;
    float vfov;

    HIPRT_HOST_DEVICE hiprtRay generateRay(uint2 reslution, uint i, uint j) {
        float3 forward = hiprt::normalize(target - from);
        float3 right = hiprt::normalize(hiprt::cross(forward, up));
        float3 camUp = hiprt::normalize(hiprt::cross(right, forward));

        float aspect = float(reslution.x) / float(reslution.y);
        float px = (2 * ((i + 0.5f) / reslution.x) - 1) * tan(vfov / 2) * aspect;
        float py = (1 - 2 * ((j + 0.5f) / reslution.y)) * tan(vfov/ 2);

        hiprtRay ray;
        ray.origin = from;
        ray.direction = hiprt::normalize(forward + right * px + camUp * py);
        return ray;
    }
};
