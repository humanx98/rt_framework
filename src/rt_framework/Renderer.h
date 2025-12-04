#pragma once

#include <filesystem>
#include <glm/glm.hpp>
#include <vector>

namespace rtf {
class OnlyMovable {
  OnlyMovable(const OnlyMovable &) = delete;
  OnlyMovable &operator=(const OnlyMovable &) = delete;

protected:
  ~OnlyMovable() = default;
  OnlyMovable() = default;
  OnlyMovable(OnlyMovable &&) noexcept = default;
  OnlyMovable &operator=(OnlyMovable &&) noexcept = default;
};

struct TriangleMesh {
  std::vector<glm::vec3> vertices;
  std::vector<glm::uvec3> triangles;

  TriangleMesh(std::vector<glm::vec3> &&vertices,
               std::vector<glm::uvec3> &&triangles)
      : vertices(std::move(vertices)), triangles(std::move(triangles)) {}
};

struct Transform {
  glm::mat4 matrix;
  float time;
};

struct Instance {
  glm::uint triangleMeshIndex;
  std::vector<Transform> transforms;

  Instance(glm::uint triangleMeshIndex, const Transform &transform)
      : triangleMeshIndex(triangleMeshIndex), transforms({transform}) {}

  Instance(glm::uint triangleMeshIndex, std::vector<Transform> &&transforms)
      : triangleMeshIndex(triangleMeshIndex),
        transforms(std::move(transforms)) {}
};

struct Scene {
  std::vector<TriangleMesh> meshes;
  std::vector<Instance> instances;
};

struct Camera {
  glm::vec3 lookFrom;
  glm::vec3 lookAt;
  glm::vec3 up;
  float vfov;
};

struct Renderer : public OnlyMovable {
  virtual ~Renderer() {}

  virtual bool initialize(int deviceIndex) = 0;
  virtual bool prepareRenderingPipeline() = 0;
  virtual bool renderFrame() = 0;
  virtual void getFrameData(std::vector<glm::vec4> &frameData) = 0;
};
} // namespace rtf
