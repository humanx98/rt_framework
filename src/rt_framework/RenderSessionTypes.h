#pragma once
#include "Renderer.h"
#include <filesystem>
#include <glm/glm.hpp>


namespace rtf {
enum class RenderBackend { None, Hiprt, Optix };

struct RenderSessionOptions {
  RenderBackend backend{};
  bool enableMotionBlur{false};
  std::filesystem::path outputPath{"result.png"};
  Scene scene{};
  Camera camera{};
  glm::uvec2 resolution{800, 600};
  int deviceId{0};
};

}