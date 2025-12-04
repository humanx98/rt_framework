
#include "OptixRenderer.h"

#include "../Utils.h"
#include "OptixLaunchParams.h"


#include <cuda.h>
#include <cuda_runtime.h>

#include <optix.h>
#include <optix_stack_size.h>
// #define OPTIX_DEFINE_FUNCTION_TABLE
#include <optix_stubs.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <print>
#include <stdexcept>
#include <string>
#include <vector>

#include <glm/gtc/type_ptr.hpp>

OptixFunctionTable g_optixFunctionTable = {};

namespace rtf {
namespace {

#define CUDA_CHECK(call)                                                       \
  do {                                                                         \
    CUresult _cuda_result = (call);                                            \
    if (_cuda_result != CUDA_SUCCESS) {                                        \
      const char *errName = nullptr;                                           \
      const char *errString = nullptr;                                         \
      cuGetErrorName(_cuda_result, &errName);                                  \
      cuGetErrorString(_cuda_result, &errString);                              \
      std::println("[cuda][error] {}: {}", errName ? errName : "Unknown",      \
                   errString ? errString : "No description");                  \
      std::abort();                                                            \
    }                                                                          \
  } while (0)

#define OPTIX_CHECK(call)                                                      \
  do {                                                                         \
    OptixResult _optix_result = (call);                                        \
    if (_optix_result != OPTIX_SUCCESS) {                                      \
      const char *errName = optixGetErrorName(_optix_result);                  \
      const char *errString = optixGetErrorString(_optix_result);              \
      std::println("[optix][error] {}: {}", errName ? errName : "Unknown",     \
                   errString ? errString : "No description");                  \
    }                                                                          \
  } while (0)

constexpr std::array<float, 12> kIdentityTransform = {
    1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,
};

template <typename T> struct alignas(OPTIX_SBT_RECORD_ALIGNMENT) SbtRecord {
  char header[OPTIX_SBT_RECORD_HEADER_SIZE];
  T data;
};

struct RaygenData {};
struct MissData {};
struct HitData {};

} // namespace

OptixRenderer::OptixRenderer(RenderSession *session) : m_session(session) {}

OptixRenderer::~OptixRenderer() {
  if (d_launchParams)
    CUDA_CHECK(cuMemFree(d_launchParams));
  if (d_output)
    CUDA_CHECK(cuMemFree(d_output));

  if (d_raygenRecord)
    CUDA_CHECK(cuMemFree(d_raygenRecord));
  if (d_missRecord)
    CUDA_CHECK(cuMemFree(d_missRecord));
  if (d_hitRecord)
    CUDA_CHECK(cuMemFree(d_hitRecord));

  for (auto &mesh : meshes) {
    if (mesh.vertices)
      CUDA_CHECK(cuMemFree(mesh.vertices));
    if (mesh.indices)
      CUDA_CHECK(cuMemFree(mesh.indices));
    if (mesh.gasBuffer)
      CUDA_CHECK(cuMemFree(mesh.gasBuffer));
  }

  for (auto &motion : motionTransforms) {
    if (motion.devicePtr)
      CUDA_CHECK(cuMemFree(motion.devicePtr));
  }

  if (tlasBuffer)
    CUDA_CHECK(cuMemFree(tlasBuffer));

  if (pipeline)
    OPTIX_CHECK(optixPipelineDestroy(pipeline));
  if (raygenPG)
    OPTIX_CHECK(optixProgramGroupDestroy(raygenPG));
  if (missPG)
    OPTIX_CHECK(optixProgramGroupDestroy(missPG));
  if (hitPG)
    OPTIX_CHECK(optixProgramGroupDestroy(hitPG));
  if (module)
    OPTIX_CHECK(optixModuleDestroy(module));
  if (optixContext)
    OPTIX_CHECK(optixDeviceContextDestroy(optixContext));
  if (stream)
    CUDA_CHECK(cuStreamDestroy(stream));
  if (cudaContext)
    CUDA_CHECK(cuCtxDestroy(cudaContext));
}

bool OptixRenderer::initialize(int deviceIndex) {
  params = {};
  params.samplesPerPixel = 32u;
  params.flipY = 0u;

  return createContext(deviceIndex);
  // createModuleAndPipeline();
  // buildScene(scene);
  // allocateOutputBuffer();
  // allocateLaunchParams();
}

bool OptixRenderer::prepareRenderingPipeline() {
  createModuleAndPipeline();
  buildScene(m_session->getScene());
  allocateOutputBuffer();
  allocateLaunchParams();
  return true;
}

void OptixRenderer::getFrameData(std::vector<glm::vec4> &frameData) {
  const auto &resolution = m_session->getResolution();
  size_t pixelCount =
      static_cast<size_t>(resolution.x) * static_cast<size_t>(resolution.y);
  if (frameData.size() < pixelCount) {
    frameData.resize(pixelCount);
  }

  CUDA_CHECK(
      cuMemcpyDtoH(frameData.data(), d_output, pixelCount * sizeof(glm::vec4)));
}

bool OptixRenderer::renderFrame() {
  const auto &resolution = m_session->getResolution();
  const auto &camera = m_session->getCamera();

  params.scene = tlasHandle;
  params.width = resolution.x;
  params.height = resolution.y;
  params.camera.lookFrom =
      make_float3(camera.lookFrom.x, camera.lookFrom.y, camera.lookFrom.z);
  params.camera.lookAt =
      make_float3(camera.lookAt.x, camera.lookAt.y, camera.lookAt.z);
  params.camera.up = make_float3(camera.up.x, camera.up.y, camera.up.z);
  params.camera.vfov = camera.vfov;
  params.colorBuffer = reinterpret_cast<float4 *>(d_output);

  CUDA_CHECK(cuMemcpyHtoD(d_launchParams, &params, sizeof(OptixLaunchParams)));

  OPTIX_CHECK(optixLaunch(pipeline, stream, d_launchParams,
                          sizeof(OptixLaunchParams), &sbt, resolution.x,
                          resolution.y, 1));

  CUDA_CHECK(cuStreamSynchronize(stream));
  return true;
}
// void OptixRenderer::render(const Camera& camera) {
//     params.scene = tlasHandle;
//     params.width = resolution.x;
//     params.height = resolution.y;
//     params.camera.lookFrom = make_float3(camera.lookFrom);
//     params.camera.lookAt = make_float3(camera.lookAt);
//     params.camera.up = make_float3(camera.up);
//     params.camera.vfov = camera.vfov;
//     params.colorBuffer = reinterpret_cast<float4*>(d_output);

//     CUDA_CHECK(cuMemcpyHtoD(d_launchParams, &params,
//     sizeof(OptixLaunchParams)));

//     OPTIX_CHECK(optixLaunch(
//         pipeline,
//         stream,
//         d_launchParams,
//         sizeof(OptixLaunchParams),
//         &sbt,
//         resolution.x,
//         resolution.y,
//         1
//     ));

//     CUDA_CHECK(cuStreamSynchronize(stream));
// }

// void OptixRenderer::getPixels(std::vector<glm::vec4>& out) {
//     size_t pixelCount = static_cast<size_t>(resolution.x) *
//     static_cast<size_t>(resolution.y); if (out.size() < pixelCount) {
//         out.resize(pixelCount);
//     }

//     CUDA_CHECK(cuMemcpyDtoH(out.data(), d_output, pixelCount *
//     sizeof(glm::vec4)));
// }

bool OptixRenderer::createContext(int deviceIndex) {
  CUDA_CHECK(cuInit(0));

  CUdevice device;
  CUDA_CHECK(cuDeviceGet(&device, deviceIndex));

  char name[256] = {};
  CUDA_CHECK(cuDeviceGetName(name, sizeof(name), device));
  std::println("OptiX renderer using device {}: {}", deviceIndex, name);

  CUDA_CHECK(cuCtxCreate(&cudaContext, 0, device));
  CUDA_CHECK(cuStreamCreate(&stream, CU_STREAM_DEFAULT));

  OPTIX_CHECK(optixInit());
  OptixDeviceContextOptions contextOptions = {};
  contextOptions.logCallbackFunction = optixLogCallback;
  contextOptions.logCallbackLevel = 4;
  OPTIX_CHECK(
      optixDeviceContextCreate(cudaContext, &contextOptions, &optixContext));

  return true;
}

void OptixRenderer::createModuleAndPipeline() {
  const std::filesystem::path ptxPath =
      resolveDevicePath(rtf::RenderBackend::Optix, "OptixMotionBlur.ptx");
  const std::string ptx = readFile(ptxPath);

  pipelineCompileOptions = {};
  pipelineCompileOptions.usesMotionBlur = true;
  pipelineCompileOptions.traversableGraphFlags =
      OPTIX_TRAVERSABLE_GRAPH_FLAG_ALLOW_ANY;
  pipelineCompileOptions.numPayloadValues = 3;
  pipelineCompileOptions.numAttributeValues = 2;
  pipelineCompileOptions.exceptionFlags = OPTIX_EXCEPTION_FLAG_NONE;
  pipelineCompileOptions.pipelineLaunchParamsVariableName = "optixLaunchParams";

  char *outputLog = new char[4 * 1024 * 1024];
  size_t sizeof_log = 4 * 1024 * 1024;

  {
    moduleCompileOptions = {};
    moduleCompileOptions.maxRegisterCount =
        OPTIX_COMPILE_DEFAULT_MAX_REGISTER_COUNT;
    moduleCompileOptions.optLevel = OPTIX_COMPILE_OPTIMIZATION_DEFAULT;
    moduleCompileOptions.debugLevel =
        OPTIX_COMPILE_DEBUG_LEVEL_NONE; // OPTIX_COMPILE_DEBUG_LEVEL_LINEINFO;
    OptixResult result = optixModuleCreate(
        optixContext, &moduleCompileOptions, &pipelineCompileOptions,
        (const char *)ptx.c_str(), strlen((const char *)ptx.c_str()), outputLog,
        &sizeof_log, &module);
    assert(result == OPTIX_SUCCESS);
  }
  std::println("OptiX module creation log:\n{}", outputLog);

  createProgramGroups();
  linkPipeline();
  createShaderBindingTable();
}

void OptixRenderer::createProgramGroups() {
  OptixProgramGroupOptions programGroupOptions = {};

  OptixProgramGroupDesc raygenDesc = {};
  raygenDesc.kind = OPTIX_PROGRAM_GROUP_KIND_RAYGEN;
  raygenDesc.raygen.module = module;
  raygenDesc.raygen.entryFunctionName = "__raygen__motion_blur";
  OPTIX_CHECK(optixProgramGroupCreate(optixContext, &raygenDesc, 1,
                                      &programGroupOptions, nullptr, nullptr,
                                      &raygenPG));

  OptixProgramGroupDesc missDesc = {};
  missDesc.kind = OPTIX_PROGRAM_GROUP_KIND_MISS;
  missDesc.miss.module = module;
  missDesc.miss.entryFunctionName = "__miss__constant";
  OPTIX_CHECK(optixProgramGroupCreate(optixContext, &missDesc, 1,
                                      &programGroupOptions, nullptr, nullptr,
                                      &missPG));

  OptixProgramGroupDesc hitDesc = {};
  hitDesc.kind = OPTIX_PROGRAM_GROUP_KIND_HITGROUP;
  hitDesc.hitgroup.moduleCH = module;
  hitDesc.hitgroup.entryFunctionNameCH = "__closesthit__motion";
  OPTIX_CHECK(optixProgramGroupCreate(optixContext, &hitDesc, 1,
                                      &programGroupOptions, nullptr, nullptr,
                                      &hitPG));
}

void OptixRenderer::linkPipeline() {
  std::array<OptixProgramGroup, 3> programGroups = {raygenPG, missPG, hitPG};

  OptixPipelineLinkOptions linkOptions = {};
  linkOptions.maxTraceDepth = 1;

  OPTIX_CHECK(optixPipelineCreate(
      optixContext, &pipelineCompileOptions, &linkOptions, programGroups.data(),
      static_cast<unsigned int>(programGroups.size()), nullptr, nullptr,
      &pipeline));

  OptixStackSizes stackSizes = {};
  for (OptixProgramGroup group : programGroups) {
    OPTIX_CHECK(optixUtilAccumulateStackSizes(group, &stackSizes, pipeline));
  }

  uint32_t directCallableStackSizeFromTraversal = 0;
  uint32_t directCallableStackSizeFromState = 0;
  uint32_t continuationStackSize = 0;
  OPTIX_CHECK(optixUtilComputeStackSizes(
      &stackSizes, linkOptions.maxTraceDepth, 0, 0,
      &directCallableStackSizeFromTraversal, &directCallableStackSizeFromState,
      &continuationStackSize));

  OPTIX_CHECK(optixPipelineSetStackSize(
      pipeline, directCallableStackSizeFromTraversal,
      directCallableStackSizeFromState, continuationStackSize, 1));
}

void OptixRenderer::createShaderBindingTable() {
  SbtRecord<RaygenData> raygenRecord = {};
  OPTIX_CHECK(optixSbtRecordPackHeader(raygenPG, &raygenRecord));
  CUDA_CHECK(cuMemAlloc(&d_raygenRecord, sizeof(raygenRecord)));
  CUDA_CHECK(cuMemcpyHtoD(d_raygenRecord, &raygenRecord, sizeof(raygenRecord)));
  sbt.raygenRecord = d_raygenRecord;

  SbtRecord<MissData> missRecord = {};
  OPTIX_CHECK(optixSbtRecordPackHeader(missPG, &missRecord));
  CUDA_CHECK(cuMemAlloc(&d_missRecord, sizeof(missRecord)));
  CUDA_CHECK(cuMemcpyHtoD(d_missRecord, &missRecord, sizeof(missRecord)));
  sbt.missRecordBase = d_missRecord;
  sbt.missRecordStrideInBytes = sizeof(missRecord);
  sbt.missRecordCount = 1;

  SbtRecord<HitData> hitRecord = {};
  OPTIX_CHECK(optixSbtRecordPackHeader(hitPG, &hitRecord));
  CUDA_CHECK(cuMemAlloc(&d_hitRecord, sizeof(hitRecord)));
  CUDA_CHECK(cuMemcpyHtoD(d_hitRecord, &hitRecord, sizeof(hitRecord)));
  sbt.hitgroupRecordBase = d_hitRecord;
  sbt.hitgroupRecordStrideInBytes = sizeof(hitRecord);
  sbt.hitgroupRecordCount = 1;
}

void OptixRenderer::buildScene(const Scene &scene) {
  buildMeshes(scene);
  buildInstances(scene);
}

void OptixRenderer::buildMeshes(const Scene &scene) {
  meshes.clear();
  meshes.reserve(scene.meshes.size());

  static const uint32_t kGeomFlags = OPTIX_GEOMETRY_FLAG_DISABLE_ANYHIT;

  for (const TriangleMesh &mesh : scene.meshes) {
    MeshBuffers buffers;

    const size_t vertexBytes = mesh.vertices.size() * sizeof(glm::vec3);
    const size_t indexBytes = mesh.triangles.size() * sizeof(glm::uvec3);

    CUDA_CHECK(cuMemAlloc(&buffers.vertices, vertexBytes));
    CUDA_CHECK(
        cuMemcpyHtoD(buffers.vertices, mesh.vertices.data(), vertexBytes));

    CUDA_CHECK(cuMemAlloc(&buffers.indices, indexBytes));
    CUDA_CHECK(
        cuMemcpyHtoD(buffers.indices, mesh.triangles.data(), indexBytes));

    OptixBuildInput buildInput = {};
    buildInput.type = OPTIX_BUILD_INPUT_TYPE_TRIANGLES;
    auto &triangles = buildInput.triangleArray;
    triangles.vertexBuffers = &buffers.vertices;
    triangles.numVertices = static_cast<unsigned int>(mesh.vertices.size());
    triangles.vertexFormat = OPTIX_VERTEX_FORMAT_FLOAT3;
    triangles.vertexStrideInBytes = sizeof(glm::vec3);
    triangles.indexBuffer = buffers.indices;
    triangles.numIndexTriplets =
        static_cast<unsigned int>(mesh.triangles.size());
    triangles.indexFormat = OPTIX_INDICES_FORMAT_UNSIGNED_INT3;
    triangles.indexStrideInBytes = sizeof(glm::uvec3);
    triangles.flags = &kGeomFlags;
    triangles.numSbtRecords = 1;

    OptixAccelBuildOptions buildOptions = {};
    buildOptions.buildFlags = OPTIX_BUILD_FLAG_PREFER_FAST_TRACE;
    buildOptions.operation = OPTIX_BUILD_OPERATION_BUILD;

    OptixAccelBufferSizes bufferSizes = {};
    OPTIX_CHECK(optixAccelComputeMemoryUsage(optixContext, &buildOptions,
                                             &buildInput, 1, &bufferSizes));

    CUdeviceptr tempBuffer = 0;
    CUDA_CHECK(cuMemAlloc(&tempBuffer, bufferSizes.tempSizeInBytes));
    CUDA_CHECK(cuMemAlloc(&buffers.gasBuffer, bufferSizes.outputSizeInBytes));

    OPTIX_CHECK(optixAccelBuild(
        optixContext, stream, &buildOptions, &buildInput, 1, tempBuffer,
        bufferSizes.tempSizeInBytes, buffers.gasBuffer,
        bufferSizes.outputSizeInBytes, &buffers.gasHandle, nullptr, 0));

    CUDA_CHECK(cuStreamSynchronize(stream));
    CUDA_CHECK(cuMemFree(tempBuffer));

    meshes.push_back(buffers);
  }
}

void OptixRenderer::createMotionTransform(const Instance &instance,
                                          MotionTransform &motion) {
  if (instance.transforms.empty()) {
    throw std::runtime_error("Instance is missing transforms for motion blur");
  }

  if (instance.triangleMeshIndex >= meshes.size()) {
    throw std::runtime_error("Instance references an invalid mesh index");
  }

  const size_t srcKeyCount = instance.transforms.size();
  const size_t keyCount = std::max<size_t>(2, srcKeyCount);
  const size_t transformSizeInBytes =
      sizeof(OptixMatrixMotionTransform) + (keyCount - 2) * 12 * sizeof(float);

  std::vector<uint8_t> hostBuffer(transformSizeInBytes);
  auto *matrixTransform =
      reinterpret_cast<OptixMatrixMotionTransform *>(hostBuffer.data());
  std::memset(matrixTransform, 0, transformSizeInBytes);

  matrixTransform->child = meshes[instance.triangleMeshIndex].gasHandle;
  matrixTransform->motionOptions.numKeys =
      static_cast<unsigned short>(keyCount);
  matrixTransform->motionOptions.timeBegin = instance.transforms.front().time;
  matrixTransform->motionOptions.timeEnd = instance.transforms.back().time;
  matrixTransform->motionOptions.flags = OPTIX_MOTION_FLAG_NONE;

  auto *transformArray =
      reinterpret_cast<float (*)[12]>(matrixTransform->transform);
  for (size_t key = 0; key < keyCount; ++key) {
    const size_t sourceIndex = min(key, srcKeyCount - 1);
    writeOptixMatrix(instance.transforms[sourceIndex].matrix,
                     transformArray[key]);
  }

  CUDA_CHECK(cuMemAlloc(&motion.devicePtr, transformSizeInBytes));
  CUDA_CHECK(
      cuMemcpyHtoD(motion.devicePtr, matrixTransform, transformSizeInBytes));
  OPTIX_CHECK(optixConvertPointerToTraversableHandle(
      optixContext, motion.devicePtr,
      OPTIX_TRAVERSABLE_TYPE_MATRIX_MOTION_TRANSFORM, &motion.handle));

  // return motion;
}

void OptixRenderer::buildInstances(const Scene &scene) {
  motionTransforms.clear();
  std::vector<OptixInstance> instances;
  instances.reserve(scene.instances.size());

  for (size_t i = 0; i < scene.instances.size(); ++i) {
    MotionTransform motion;
    createMotionTransform(scene.instances[i], motion);
    motionTransforms.push_back(motion);

    OptixInstance optixInstance = {};
    writeOptixMatrix(glm::mat4(1.0f), optixInstance.transform);
    optixInstance.traversableHandle = motion.handle;
    optixInstance.instanceId = static_cast<unsigned int>(i);
    optixInstance.sbtOffset = 0;
    optixInstance.visibilityMask = 0xFF;
    optixInstance.flags = OPTIX_INSTANCE_FLAG_NONE;

    instances.push_back(optixInstance);
  }

  CUdeviceptr d_instances = 0;
  const size_t instanceBytes = instances.size() * sizeof(OptixInstance);
  CUDA_CHECK(cuMemAlloc(&d_instances, instanceBytes));
  CUDA_CHECK(cuMemcpyHtoD(d_instances, instances.data(), instanceBytes));

  OptixBuildInput buildInput = {};
  buildInput.type = OPTIX_BUILD_INPUT_TYPE_INSTANCES;
  buildInput.instanceArray.instances = d_instances;
  buildInput.instanceArray.numInstances =
      static_cast<unsigned int>(instances.size());

  OptixAccelBuildOptions buildOptions = {};
  buildOptions.buildFlags = OPTIX_BUILD_FLAG_PREFER_FAST_TRACE;
  buildOptions.operation = OPTIX_BUILD_OPERATION_BUILD;

  OptixAccelBufferSizes bufferSizes = {};
  OPTIX_CHECK(optixAccelComputeMemoryUsage(optixContext, &buildOptions,
                                           &buildInput, 1, &bufferSizes));

  CUdeviceptr tempBuffer = 0;
  CUDA_CHECK(cuMemAlloc(&tempBuffer, bufferSizes.tempSizeInBytes));
  CUDA_CHECK(cuMemAlloc(&tlasBuffer, bufferSizes.outputSizeInBytes));

  OPTIX_CHECK(optixAccelBuild(optixContext, stream, &buildOptions, &buildInput,
                              1, tempBuffer, bufferSizes.tempSizeInBytes,
                              tlasBuffer, bufferSizes.outputSizeInBytes,
                              &tlasHandle, nullptr, 0));

  CUDA_CHECK(cuStreamSynchronize(stream));
  CUDA_CHECK(cuMemFree(tempBuffer));
  CUDA_CHECK(cuMemFree(d_instances));
}

void OptixRenderer::allocateOutputBuffer() {
  const auto &resolution = m_session->getResolution();
  const size_t pixelCount =
      static_cast<size_t>(resolution.x) * static_cast<size_t>(resolution.y);
  const size_t bufferBytes = pixelCount * sizeof(glm::vec4);
  CUDA_CHECK(cuMemAlloc(&d_output, bufferBytes));
}

void OptixRenderer::allocateLaunchParams() {
  CUDA_CHECK(cuMemAlloc(&d_launchParams, sizeof(OptixLaunchParams)));
}

} // namespace rtf
