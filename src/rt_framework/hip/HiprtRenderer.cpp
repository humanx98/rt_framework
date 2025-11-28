#include "HiprtRenderer.h"
#include <cstdint>
#include <hiprt/hiprt.h>
#include <hiprt/hiprt_types.h>
#include <print>
#include "HipMisc.h"
#include "glm/fwd.hpp"
#include <hip/hip_runtime.h>
#include <vector>

namespace rtf {
    static hiprtFrameMatrix glmToHiprt(const glm::mat4& mat, float time) {
        hiprtFrameMatrix matrix = {
            .matrix = {
                { mat[0][0], mat[1][0], mat[2][0], mat[3][0] },
                { mat[0][1], mat[1][1], mat[2][1], mat[3][1] },
                { mat[0][2], mat[1][2], mat[2][2], mat[3][2] },
            },
            .time = time,
        };
        return matrix;
    }

    HiprtRenderer::~HiprtRenderer() {
        if (scene) HIPRT_CHECK(hiprtDestroyScene(hiprtContext, scene));

        for (auto geometry : geometries) {
            HIPRT_CHECK(hiprtDestroyGeometry(hiprtContext, geometry));
        }

        for (auto ptr : hipGC) {
            HIP_CHECK(hipFree(ptr));
        }

        if (module) HIP_CHECK(hipModuleUnload(module));
        if (hiprtContext) HIPRT_CHECK(hiprtDestroyContext(hiprtContext));
        DISABLE_DEPRECATED_WARNINGS
        if (hipContext) HIP_CHECK(hipCtxDestroy(hipContext));
        ENABLE_DEPRECATED_WARNINGS
    }

    void HiprtRenderer::init(int deviceIndex, glm::uvec2 resolution, const Scene& scene) {
        initContext(deviceIndex);
        initScene(scene);
        this->resolution = resolution;
        this->pixels = hipGCAlloc<glm::vec4>(resolution.x * resolution.y);
        HIP_CHECK(hipModuleLoad(&module, "hip/device/RenderNormalsPrimitives.hipfb"));
        // HIP_CHECK(hipModuleGetFunction(&func, module, "RenderNormals"));
        // HIP_CHECK(hipModuleGetFunction(&func, module, "RenderPrimitives"));
        HIP_CHECK(hipModuleGetFunction(&func, module, "RenderMotionBlur"));
    }

    void HiprtRenderer::initContext(int deviceIndex) {
        std::println("Device index: {}", deviceIndex);
        HIP_CHECK(hipInit(0));
        HIP_CHECK(hipSetDevice(deviceIndex));
        DISABLE_DEPRECATED_WARNINGS
        HIP_CHECK(hipCtxCreate(&hipContext, 0, deviceIndex));
        ENABLE_DEPRECATED_WARNINGS
        hipDeviceProp_t props;
        HIP_CHECK(hipGetDeviceProperties(&props, deviceIndex));

        std::println("Device name: {}", props.name);
        hiprtContextCreationInput contextCreationInput = {
            .ctxt = hipContext,
            .device = deviceIndex,
            .deviceType = std::string(props.name).find("NVIDIA") != std::string::npos
                ? hiprtDeviceNVIDIA
                : hiprtDeviceAMD
        };
        HIPRT_CHECK(hiprtCreateContext(HIPRT_API_VERSION, contextCreationInput, hiprtContext));
    }

    void HiprtRenderer::initScene(const Scene& scene) {
        hiprtBuildOptions buildOptions = { .buildFlags = hiprtBuildFlagBitPreferHighQualityBuild };
        for (const auto& mesh : scene.meshes) {
            hiprtGeometryBuildInput buildInput = { .geomType = hiprtPrimitiveTypeTriangleMesh };
            buildInput.primitive.triangleMesh = {
                .vertices = hipGCAlloc(mesh.vertices),
                .vertexCount = (uint32_t)mesh.vertices.size(),
                .vertexStride = sizeof(mesh.vertices[0]),
                .triangleIndices = hipGCAlloc(mesh.triangles),
                .triangleCount = (uint32_t)mesh.triangles.size(),
                .triangleStride = sizeof(mesh.triangles[0])
            };

            size_t tempBuffSize;
            HIPRT_CHECK(hiprtGetGeometryBuildTemporaryBufferSize(hiprtContext, buildInput, buildOptions, tempBuffSize));
            hipDeviceptr_t tempBuff = hipGCAlloc<uint8_t>(tempBuffSize);

            hiprtGeometry geometry;
            HIPRT_CHECK(hiprtCreateGeometry(hiprtContext, buildInput, buildOptions, geometry));
            geometries.push_back(geometry);
            HIPRT_CHECK(hiprtBuildGeometry(hiprtContext, hiprtBuildOperationBuild, buildInput, buildOptions, tempBuff, nullptr, geometry));
        }

        std::vector<hiprtInstance> instances;
        std::vector<hiprtTransformHeader> transformHeaders;
        std::vector<hiprtFrameMatrix> frameMatrices;

        instances.reserve(scene.instances.size());
        transformHeaders.reserve(scene.instances.size());
        frameMatrices.reserve(scene.instances.size());

        for (const auto& instance : scene.instances) {
            instances.push_back({
                .type = hiprtInstanceTypeGeometry,
                .geometry = geometries[instance.triangleMeshIndex]
            });

            transformHeaders.push_back({
                .frameIndex = (uint32_t)frameMatrices.size(),
                .frameCount = (uint32_t)instance.transforms.size()
            });

            for (const auto& transform : instance.transforms) {
                frameMatrices.push_back(glmToHiprt(transform.matrix, transform.time));
            }
        }

        hiprtSceneBuildInput buildInput = {
            .instances = hipGCAlloc(instances),
            .instanceTransformHeaders = hipGCAlloc(transformHeaders),
            .instanceFrames = hipGCAlloc(frameMatrices),
            .instanceCount = (uint32_t)instances.size(),
            .frameCount = (uint32_t)frameMatrices.size(),
            .frameType = hiprtFrameTypeMatrix
        };
        size_t tempBuffSize;
        HIPRT_CHECK(hiprtGetSceneBuildTemporaryBufferSize(hiprtContext, buildInput, buildOptions, tempBuffSize));
        hipDeviceptr_t tempBuff = hipGCAlloc<uint8_t>(tempBuffSize);

        HIPRT_CHECK(hiprtCreateScene(hiprtContext, buildInput, buildOptions, this->scene));
        HIPRT_CHECK(hiprtBuildScene(hiprtContext, hiprtBuildOperationBuild, buildInput, buildOptions, tempBuff, nullptr, this->scene));
    }

    void HiprtRenderer::render(const Camera& camera) {
        bool flipY = false;
        void* args[] = { &scene, (void*)&camera, &pixels, &resolution, &flipY};
        uint3 block = { 1024, 1, 1 };
        uint3 grid = { ((resolution.x * resolution.y) + block.x - 1) / block.x, 1, 1 };
        HIP_CHECK(hipModuleLaunchKernel(func, grid.x, grid.y, grid.z, block.x, block.y, block.z, 0, nullptr, args, 0));
    }

    void HiprtRenderer::getPixels(std::vector<glm::vec4>& out) {
        size_t pixelCount = resolution.x * resolution.y;
        if (out.size() < pixelCount) {
            out.resize(pixelCount);
        }
        HIP_CHECK(hipMemcpyDtoH(out.data(), pixels, pixelCount * sizeof(out[0])));
    }
}
