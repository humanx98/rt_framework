#pragma once
#include "Renderer.h"
#include <filesystem>
#include <memory>
#include "RenderSessionTypes.h"


namespace rtf {

class RenderSession {
public:
  RenderSession() = default;
  ~RenderSession() = default;
  static std::unique_ptr<RenderSession> create();

  bool initialize(const RenderSessionOptions &options);
  bool prepareRenderingPipeline();

  const Scene& getScene() const {return m_scene; }
  const Camera& getCamera() const {return m_camera;}
  const std::vector<glm::mat4>& getTransforms() const {return m_transforms;}
  bool isMotionBlurEnabled() const {return m_enableMotionBlur;}

private:
  bool createRenderer(RenderBackend backend, RenderSession* session);

  std::unique_ptr<Renderer> m_renderer_impl{nullptr};

protected:
  Scene m_scene{};
  Camera m_camera{};
  std::vector<glm::mat4> m_transforms{};

  std::filesystem::path m_outputPath{};
  bool m_enableMotionBlur{false};
};

} // namespace rtf