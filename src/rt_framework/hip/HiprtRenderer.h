#pragma once

#include "../Renderer.h"
#include "../RenderSession.h"
#include <hip/hip_runtime.h>
#include <hiprt/hiprt.h>
#include <vector>
#include "HipMisc.h"

namespace rtf {
    class HiprtRenderer : public Renderer {
    public:
        HiprtRenderer() = default;
        HiprtRenderer(RenderSession* session);
        ~HiprtRenderer();
        virtual bool initialize(int deviceIndex) override;
        virtual bool prepareRenderingPipeline() override;
        virtual bool renderFrame() override;
        virtual void getFrameData(std::vector<glm::vec4>& frameData) override;
        
    private:
        RenderSession* m_session{nullptr};

        bool createContext(int deviceIndex);
        bool createGAS();
        bool createIAS();
        bool finalizeScene();
        bool allocateOutputBuffer();

        bool prepareModules();
       
    
        std::vector<hiprtInstance> m_instances;
        std::vector<hiprtTransformHeader> m_transformHeaders;
        std::vector<hiprtFrameMatrix> m_frameMatrices;


        hipCtx_t m_hipContext = nullptr;
        hiprtContext m_hiprtContext = nullptr;
        hipStream_t m_hipStream = nullptr;

        std::unordered_map<std::string, hipModule_t> m_modules;
        std::unordered_map<std::string, hipFunction_t> m_functions;

        std::vector<hiprtGeometry> m_geometries;
        hiprtScene m_scene = nullptr;
        hipDeviceptr_t m_outputBuffer = nullptr;
        glm::uvec2 m_resolution {0, 0};
        bool m_pipeline_ready{false};
        
        std::vector<hipDeviceptr_t> m_hipGC;
                template<typename T>
        hipDeviceptr_t hipGCAlloc(size_t count) {
            hipDeviceptr_t ptr;
            HIP_CHECK(hipMalloc(&ptr, count * sizeof(T)));
            m_hipGC.push_back(ptr);
            return ptr;
        }
        template<typename T>
        hipDeviceptr_t hipGCAlloc(const std::vector<T>& src) {
            hipDeviceptr_t ptr = hipGCAlloc<T>(src.size());
            HIP_CHECK(hipMemcpyHtoD(ptr, (void*)src.data(), src.size() * sizeof(T)));
            return ptr;
        }

    };
}
