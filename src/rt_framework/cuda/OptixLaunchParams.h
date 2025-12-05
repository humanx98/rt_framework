#pragma once

#include <optix.h>
#include <cuda_runtime.h>

struct OptixCameraData {
    float3 lookFrom;
    float3 lookAt;
    float3 up;
    float vfov;
};

struct OptixLaunchParams {
    float4* colorBuffer;
    OptixTraversableHandle scene;
    OptixCameraData camera;
    unsigned int width;
    unsigned int height;
    unsigned int samplesPerPixel;
    unsigned int flipY;
};
