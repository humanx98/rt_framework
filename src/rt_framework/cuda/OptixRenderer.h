#pragma once

#include "../Renderer.h"
#include "../RenderSession.h"
#include "OptixLaunchParams.h"

#include <cuda.h>
#include <optix.h>
#include <vector>

namespace rtf {
    class OptixRenderer : public Renderer {
    public:
        OptixRenderer() = default;
        OptixRenderer(RenderSession* session);
        ~OptixRenderer();

        bool initialize(int deviceIndex) override;
        bool prepareRenderingPipeline() override;
        // void render(const Camera& camera) override;
        // void getPixels(std::vector<glm::vec4>& out) override;

    private:
        RenderSession* m_session{nullptr};

        struct MeshBuffers {
            CUdeviceptr vertices = 0;
            CUdeviceptr indices = 0;
            CUdeviceptr gasBuffer = 0;
            OptixTraversableHandle gasHandle = 0;
        };

        struct MotionTransform {
            CUdeviceptr devicePtr = 0;
            OptixTraversableHandle handle = 0;
        };

        bool createContext(int deviceIndex);
        void createModuleAndPipeline();
        void createProgramGroups();
        void linkPipeline();
        void createShaderBindingTable();
        void buildScene(const Scene& scene);
        void buildMeshes(const Scene& scene);
        void createMotionTransform(const Instance& instance, MotionTransform& motion);
        void buildInstances(const Scene& scene);
        void allocateOutputBuffer();
        void allocateLaunchParams();

        CUcontext cudaContext = nullptr;
        CUstream stream = nullptr;
        OptixDeviceContext optixContext = nullptr;

        OptixModule module = nullptr;
        OptixPipeline pipeline = nullptr;
        OptixProgramGroup raygenPG = nullptr;
        OptixProgramGroup missPG = nullptr;
        OptixProgramGroup hitPG = nullptr;
        OptixShaderBindingTable sbt = {};
        OptixModuleCompileOptions moduleCompileOptions = {};
        OptixPipelineCompileOptions pipelineCompileOptions = {};

        std::vector<MeshBuffers> meshes;
        std::vector<MotionTransform> motionTransforms;
        OptixTraversableHandle tlasHandle = 0;
        CUdeviceptr tlasBuffer = 0;

        CUdeviceptr d_raygenRecord = 0;
        CUdeviceptr d_missRecord = 0;
        CUdeviceptr d_hitRecord = 0;

        CUdeviceptr d_output = 0;
        CUdeviceptr d_launchParams = 0;
        glm::uvec2 resolution = { 0, 0 };
        OptixLaunchParams params = {};
    };
}
