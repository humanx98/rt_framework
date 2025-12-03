#include "RenderSession.h"
#include "RendererFactories.h"
#include <stdexcept>

namespace rtf {

std::unique_ptr<RenderSession> RenderSession::create() {
  auto session = std::make_unique<RenderSession>();
  return session;
}
bool RenderSession::createRenderer(RenderBackend backend, RenderSession* session) {
  switch (backend) {
  case RenderBackend::Hiprt:
#if defined(USE_HIP)
    m_renderer_impl = createHiprtRenderer(session);
#else
    throw std::runtime_error("HIPRT backend not available.");
#endif
    break;
  case RenderBackend::Optix:
#if defined(USE_CUDA)
    m_renderer_impl = createOptixRenderer(session);
#else
    throw std::runtime_error("OptiX backend not available.");
#endif
    break;
  default:
    throw std::runtime_error("Unsupported render backend.");
  }
  return true;
}

bool RenderSession::initialize(const RenderSessionOptions &options) {

  m_enableMotionBlur = options.enableMotionBlur;
  m_outputPath = options.outputPath;
  m_scene = options.scene;
  m_camera = options.camera;
  if (!createRenderer(options.backend, this)) {
    return false;
  }
  m_renderer_impl->initialize(options.deviceId);

  return true;
}

bool RenderSession::prepareRenderingPipeline()
{
    return m_renderer_impl->prepareRenderingPipeline();
}

} // namespace rtf