#include "HiprtRenderer.h"
#include "../RenderSession.h"
#include "../Utils.h"
#include "HipMisc.h"
#include "glm/fwd.hpp"
#include <cstdint>
#include <hip/hip_runtime.h>
#include <hiprt/hiprt.h>
#include <hiprt/hiprt_types.h>
#include <print>
#include <vector>


namespace rtf {
static hiprtFrameMatrix glmToHiprt(const glm::mat4 &mat, float time) {
  hiprtFrameMatrix matrix = {
      .matrix =
          {
              {mat[0][0], mat[1][0], mat[2][0], mat[3][0]},
              {mat[0][1], mat[1][1], mat[2][1], mat[3][1]},
              {mat[0][2], mat[1][2], mat[2][2], mat[3][2]},
          },
      .time = time,
  };
  return matrix;
}

HiprtRenderer::HiprtRenderer(RenderSession *session) : m_session(session) {}

HiprtRenderer::~HiprtRenderer() {
  if (m_scene)
    HIPRT_CHECK(hiprtDestroyScene(m_hiprtContext, m_scene));

  for (auto geometry : m_geometries) {
    HIPRT_CHECK(hiprtDestroyGeometry(m_hiprtContext, geometry));
  }

  for (auto ptr : m_hipGC) {
    HIP_CHECK(hipFree(ptr));
  }

  for (auto &[name, m_module] : m_modules) {
    if (m_module)
      HIP_CHECK(hipModuleUnload(m_module));
  }

  if (m_hiprtContext)
    HIPRT_CHECK(hiprtDestroyContext(m_hiprtContext));
  if (m_hipContext)
    HIP_CHECK(hipCtxDestroy(m_hipContext));
  if (m_hipStream)
    HIP_CHECK(hipStreamDestroy(m_hipStream));
}

bool HiprtRenderer::initialize(int deviceIndex) {
  return createContext(deviceIndex);
}

bool HiprtRenderer::createGAS() {
  const Scene &session_scene = m_session->getScene();
  const auto &meshes = session_scene.meshes;
  constexpr hiprtBuildOptions buildOptions = {
      .buildFlags = hiprtBuildFlagBitPreferHighQualityBuild};

  if (meshes.empty()) {
    std::println("Scene does not have any meshes");
    return false;
  }

  for (const auto &mesh : meshes) {
    hiprtGeometryBuildInput buildInput = {.geomType =
                                              hiprtPrimitiveTypeTriangleMesh};

    buildInput.primitive.triangleMesh = {
        .vertices = hipGCAlloc(mesh.vertices),
        .vertexCount = (uint32_t)mesh.vertices.size(),
        .vertexStride = sizeof(mesh.vertices[0]),
        .triangleIndices = hipGCAlloc(mesh.triangles),
        .triangleCount = (uint32_t)mesh.triangles.size(),
        .triangleStride = sizeof(mesh.triangles[0])};

    size_t tempBuffSize;
    HIPRT_CHECK(hiprtGetGeometryBuildTemporaryBufferSize(
        m_hiprtContext, buildInput, buildOptions, tempBuffSize));
    hipDeviceptr_t tempBuff = hipGCAlloc<uint8_t>(tempBuffSize);

    hiprtGeometry geometry;
    HIPRT_CHECK(hiprtCreateGeometry(m_hiprtContext, buildInput, buildOptions,
                                    geometry));
    HIPRT_CHECK(hiprtBuildGeometry(m_hiprtContext, hiprtBuildOperationBuild,
                                   buildInput, buildOptions, tempBuff,
                                   m_hipStream, geometry));
    m_geometries.push_back(geometry);
  }

  return true;
}

bool HiprtRenderer::createIAS() {
  bool withMotionBlur = m_session->isMotionBlurEnabled();
  const Scene &scene = m_session->getScene();
  if (scene.instances.empty()) {
    std::println("No instances in the scene!");
    return false;
  }

  m_instances.clear();
  m_instances.reserve(scene.instances.size());
  m_transformHeaders.clear();
  m_transformHeaders.reserve(scene.instances.size());
  m_frameMatrices.clear();
  m_frameMatrices.reserve(scene.instances.size());

  for (const auto &instance : scene.instances) {
    m_instances.push_back(
        {.type = hiprtInstanceTypeGeometry,
         .geometry = m_geometries[instance.triangleMeshIndex]});

    m_transformHeaders.push_back(
        {.frameIndex = (uint32_t)m_frameMatrices.size(),
         .frameCount = (uint32_t)instance.transforms.size()});

    for (const auto &transform : instance.transforms) {
      m_frameMatrices.push_back(glmToHiprt(transform.matrix, transform.time));
    }
  }
  return true;
}

bool HiprtRenderer::finalizeScene() {
  if (m_instances.empty()) {
    std::println("Skipping building scene");
    return false;
  }

  hiprtSceneBuildInput buildInput = {
      .instances = hipGCAlloc(m_instances),
      .instanceTransformHeaders = hipGCAlloc(m_transformHeaders),
      .instanceFrames = hipGCAlloc(m_frameMatrices),
      .instanceCount = (uint32_t)m_instances.size(),
      .frameCount = (uint32_t)m_frameMatrices.size(),
      .frameType = hiprtFrameTypeMatrix};

  constexpr hiprtBuildOptions buildOptions = {
      .buildFlags = hiprtBuildFlagBitPreferHighQualityBuild};
  size_t tempBuffSize;
  HIPRT_CHECK(hiprtGetSceneBuildTemporaryBufferSize(
      m_hiprtContext, buildInput, buildOptions, tempBuffSize));
  hipDeviceptr_t tempBuff = hipGCAlloc<uint8_t>(tempBuffSize);

  HIPRT_CHECK(
      hiprtCreateScene(m_hiprtContext, buildInput, buildOptions, m_scene));
  HIPRT_CHECK(hiprtBuildScene(m_hiprtContext, hiprtBuildOperationBuild,
                              buildInput, buildOptions, tempBuff, m_hipStream,
                              m_scene));
  HIP_CHECK(hipStreamSynchronize(m_hipStream));
  return true;
}

bool HiprtRenderer::prepareModules() {
  hipModule_t module;
  HIP_CHECK(
      hipModuleLoad(&module, resolveDevicePath(rtf::RenderBackend::Hiprt,
                                               "RenderNormalsPrimitives.hipfb")
                                 .string()
                                 .c_str()));
  m_modules["main"] = module;
  std::array<const char *, 3> kernelNames{
      "RenderNormals",
      "RenderPrimitives",
      "RenderMotionBlur",
  };

  for (const auto &name : kernelNames) {
    hipFunction_t func;
    HIP_CHECK(hipModuleGetFunction(&func, module, name));
    m_functions[name] = func;
  }
  // }

  // // HIP_CHECK(hipModuleGetFunction(&func, module, "RenderNormals"));
  // // HIP_CHECK(hipModuleGetFunction(&func, module, "RenderPrimitives"));
  // HIP_CHECK(hipModuleGetFunction(&func, module, "RenderMotionBlur"));
  return true;
}

bool HiprtRenderer::allocateOutputBuffer() {
  const auto &resolution = m_session->getResolution();
  const size_t pixelCount =
      static_cast<size_t>(resolution.x) * static_cast<size_t>(resolution.y);
  const size_t bufferBytes = pixelCount * sizeof(glm::vec4);
  m_outputBuffer = hipGCAlloc<uint8_t>(bufferBytes);
  return true;
}

void HiprtRenderer::getFrameData(std::vector<glm::vec4> &frameData) {
  const auto &resolution = m_session->getResolution();
  size_t pixelCount =
      static_cast<size_t>(resolution.x) * static_cast<size_t>(resolution.y);
  if (frameData.size() < pixelCount) {
    frameData.resize(pixelCount);
  }
  HIP_CHECK(hipMemcpyDtoH(frameData.data(), m_outputBuffer,
                          frameData.size() * sizeof(glm::vec4)));
}

bool HiprtRenderer::prepareRenderingPipeline() {
  auto gas_result = createGAS();
  auto ias_result = createIAS();
  auto finalize_result = finalizeScene();
  auto modules_result = prepareModules();
  auto output_buffer_result = allocateOutputBuffer();
  m_pipeline_ready = gas_result && ias_result && finalize_result &&
                     modules_result && output_buffer_result;
  return m_pipeline_ready;
}

bool HiprtRenderer::createContext(int deviceIndex) {
  std::println("Device index: {}", deviceIndex);
  HIP_CHECK(hipInit(0));
  HIP_CHECK(hipSetDevice(deviceIndex));
  DISABLE_DEPRECATED_WARNINGS
  HIP_CHECK(hipCtxCreate(&m_hipContext, 0, deviceIndex));
  ENABLE_DEPRECATED_WARNINGS
  hipDeviceProp_t props;
  HIP_CHECK(hipGetDeviceProperties(&props, deviceIndex));

  std::println("Device name: {}", props.name);
  hiprtContextCreationInput contextCreationInput = {
      .ctxt = m_hipContext,
      .device = deviceIndex,
      .deviceType = std::string(props.name).find("NVIDIA") != std::string::npos
                        ? hiprtDeviceNVIDIA
                        : hiprtDeviceAMD};
  HIP_CHECK(hipStreamCreate(&m_hipStream));
  HIPRT_CHECK(hiprtCreateContext(HIPRT_API_VERSION, contextCreationInput,
                                 m_hiprtContext));
  return true;
}

bool HiprtRenderer::renderFrame() {
  if (!m_pipeline_ready) {
    return false;
  }
  bool flipY = false;
  const auto resolution = m_session->getResolution();
  void *args[] = {&m_scene, (void *)&m_session->getCamera(), &m_outputBuffer,
                  (void *)&resolution, &flipY};

  uint3 block = {128, 1, 1};
  uint3 grid = {((resolution.x * resolution.y) + block.x - 1) / block.x, 1, 1};
  HIP_CHECK(hipModuleLaunchKernel(m_functions["RenderMotionBlur"], grid.x,
                                  grid.y, grid.z, block.x, block.y, block.z, 0,
                                  m_hipStream, args, 0));
  return true;
}
// void HiprtRenderer::initScene(const Scene& scene) {
//     hiprtBuildOptions buildOptions = { .buildFlags =
//     hiprtBuildFlagBitPreferHighQualityBuild }; for (const auto& mesh :
//     scene.meshes) {
//         hiprtGeometryBuildInput buildInput = { .geomType =
//         hiprtPrimitiveTypeTriangleMesh }; buildInput.primitive.triangleMesh =
//         {
//             .vertices = hipGCAlloc(mesh.vertices),
//             .vertexCount = (uint32_t)mesh.vertices.size(),
//             .vertexStride = sizeof(mesh.vertices[0]),
//             .triangleIndices = hipGCAlloc(mesh.triangles),
//             .triangleCount = (uint32_t)mesh.triangles.size(),
//             .triangleStride = sizeof(mesh.triangles[0])
//         };

//         size_t tempBuffSize;
//         HIPRT_CHECK(hiprtGetGeometryBuildTemporaryBufferSize(hiprtContext,
//         buildInput, buildOptions, tempBuffSize)); hipDeviceptr_t tempBuff =
//         hipGCAlloc<uint8_t>(tempBuffSize);

//         hiprtGeometry geometry;
//         HIPRT_CHECK(hiprtCreateGeometry(hiprtContext, buildInput,
//         buildOptions, geometry)); geometries.push_back(geometry);
//         HIPRT_CHECK(hiprtBuildGeometry(hiprtContext,
//         hiprtBuildOperationBuild, buildInput, buildOptions, tempBuff,
//         nullptr, geometry));
//     }

//     std::vector<hiprtInstance> instances;
//     std::vector<hiprtTransformHeader> transformHeaders;
//     std::vector<hiprtFrameMatrix> frameMatrices;

//     instances.reserve(scene.instances.size());
//     transformHeaders.reserve(scene.instances.size());
//     frameMatrices.reserve(scene.instances.size());

//     for (const auto& instance : scene.instances) {
//         instances.push_back({
//             .type = hiprtInstanceTypeGeometry,
//             .geometry = geometries[instance.triangleMeshIndex]
//         });

//         transformHeaders.push_back({
//             .frameIndex = (uint32_t)frameMatrices.size(),
//             .frameCount = (uint32_t)instance.transforms.size()
//         });

//         for (const auto& transform : instance.transforms) {
//             frameMatrices.push_back(glmToHiprt(transform.matrix,
//             transform.time));
//         }
//     }

//     hiprtSceneBuildInput buildInput = {
//         .instances = hipGCAlloc(instances),
//         .instanceTransformHeaders = hipGCAlloc(transformHeaders),
//         .instanceFrames = hipGCAlloc(frameMatrices),
//         .instanceCount = (uint32_t)instances.size(),
//         .frameCount = (uint32_t)frameMatrices.size(),
//         .frameType = hiprtFrameTypeMatrix
//     };
//     size_t tempBuffSize;
//     HIPRT_CHECK(hiprtGetSceneBuildTemporaryBufferSize(hiprtContext,
//     buildInput, buildOptions, tempBuffSize)); hipDeviceptr_t tempBuff =
//     hipGCAlloc<uint8_t>(tempBuffSize);

//     HIPRT_CHECK(hiprtCreateScene(hiprtContext, buildInput, buildOptions,
//     this->scene)); HIPRT_CHECK(hiprtBuildScene(hiprtContext,
//     hiprtBuildOperationBuild, buildInput, buildOptions, tempBuff, nullptr,
//     this->scene));
// }

// void HiprtRenderer::render(const Camera& camera) {
//     bool flipY = false;
//     void* args[] = { &scene, (void*)&camera, &pixels, &resolution, &flipY};
//     uint3 block = { 1024, 1, 1 };
//     uint3 grid = { ((resolution.x * resolution.y) + block.x - 1) / block.x,
//     1, 1 }; HIP_CHECK(hipModuleLaunchKernel(func, grid.x, grid.y, grid.z,
//     block.x, block.y, block.z, 0, nullptr, args, 0));
// }

// void HiprtRenderer::getPixels(std::vector<glm::vec4>& out) {
//     size_t pixelCount = resolution.x * resolution.y;
//     if (out.size() < pixelCount) {
//         out.resize(pixelCount);
//     }
//     HIP_CHECK(hipMemcpyDtoH(out.data(), pixels, pixelCount *
//     sizeof(out[0])));
// }
} // namespace rtf
