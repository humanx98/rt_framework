#pragma once

#include "../Renderer.h"
#include <hip/hip_runtime.h>
#include <hiprt/hiprt.h>
#include <vector>
#include "HipMisc.h"

namespace rtf {
    class HiprtRenderer : public Renderer {
    public:
        ~HiprtRenderer();
        virtual void init(int deviceIndex, glm::uvec2 resolution, const Scene& scene) override;
        virtual void render(const Camera& camera) override;
        virtual void getPixels(std::vector<glm::vec4>& pixels) override;

    private:
        void initContext(int deviceIndex);
        void initScene(const Scene& scene);
        template<typename T>
        hipDeviceptr_t hipGCAlloc(size_t count) {
            hipDeviceptr_t ptr;
            HIP_CHECK(hipMalloc(&ptr, count * sizeof(T)));
            hipGC.push_back(ptr);
            return ptr;
        }
        template<typename T>
        hipDeviceptr_t hipGCAlloc(const std::vector<T>& src) {
            hipDeviceptr_t ptr = hipGCAlloc<T>(src.size());
            HIP_CHECK(hipMemcpyHtoD(ptr, (void*)src.data(), src.size() * sizeof(T)));
            return ptr;
        }

        hipCtx_t hipContext = nullptr;
        hiprtContext hiprtContext = nullptr;
        hipModule_t module = nullptr;
        hipFunction_t func = nullptr;
        std::vector<hiprtGeometry> geometries;
        std::vector<hipDeviceptr_t> hipGC;
        hiprtScene scene = nullptr;
        hipDeviceptr_t pixels = nullptr;
        glm::uvec2 resolution;
    };
}
