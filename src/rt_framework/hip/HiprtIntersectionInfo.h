#pragma once

#include <hip/hip_runtime.h>
#include <cstdint>

namespace rtf {
struct MeshInfo {
    float3* vertices{nullptr};
    uint3* vertexIds{nullptr};
    uint32_t vertexCount{0};
    uint32_t triangleCount{0};
};

struct InstanceTransform {
    float4 rows[4]{};
    float time{0.0f};
};

struct IntersectionInfo {
    MeshInfo* mesh{nullptr};
    InstanceTransform* transforms{nullptr};
    uint32_t transformCount{0};
};
} // namespace rtf
