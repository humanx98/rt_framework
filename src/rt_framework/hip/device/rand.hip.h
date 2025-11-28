#pragma once

#include <hip/hip_runtime.h>
#include <hiprt/hiprt_device.h>

HIPRT_HOST_DEVICE HIPRT_INLINE uint32_t rand_u32(uint32_t& state)
{
    // PCG random number generator
    // Based on https://www.shadertoy.com/view/XlGcRh

    // rng_state = (word >> 22u) ^ word;
    // return rng_state;
    uint32_t oldState = state + 747796405 + 2891336453;
    uint32_t word = ((oldState >> ((oldState >> 28) + 4)) ^ oldState) * 277803737;
    state = (word >> 22) ^ word;
    return state;
}

HIPRT_HOST_DEVICE HIPRT_INLINE float rand_f32(uint32_t& state) { return rand_u32(state) * (1.0f / 4294967296.0f); }
HIPRT_HOST_DEVICE HIPRT_INLINE float rand_f32(uint32_t& state, float min, float max) { return min + (max - min) * rand_f32(state); }
