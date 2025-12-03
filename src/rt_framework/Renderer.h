#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <filesystem>

namespace rtf {
    class OnlyMovable {
        OnlyMovable(const OnlyMovable&) = delete;
        OnlyMovable& operator=(const OnlyMovable&) = delete;
    protected:
        ~OnlyMovable() = default;
        OnlyMovable() = default;
        OnlyMovable(OnlyMovable&&) noexcept = default;
        OnlyMovable& operator=(OnlyMovable&&) noexcept = default;
    };

    struct TriangleMesh {
        std::vector<glm::vec3> vertices;
        std::vector<glm::uvec3> triangles;

        TriangleMesh(std::vector<glm::vec3>&& vertices, std::vector<glm::uvec3>&& triangles)
            : vertices(std::move(vertices))
            , triangles(std::move(triangles)) {
        }
    };

    struct Transform {
        glm::mat4 matrix;
        float time;
    };

    struct Instance {
        glm::uint triangleMeshIndex;
        std::vector<Transform> transforms;

        Instance(glm::uint triangleMeshIndex, const Transform& transform)
            : triangleMeshIndex(triangleMeshIndex)
            , transforms({ transform }){
        }

        Instance(glm::uint triangleMeshIndex, std::vector<Transform>&& transforms)
            : triangleMeshIndex(triangleMeshIndex)
            , transforms(std::move(transforms)){
        }
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

        // virtual void addMesh(const TriangleMesh &mesh) = 0;

        // virtual void setInstanceTransform(glm::mat4 &transform) = 0;
        // virtual void setMotionTransforms(std::vector<glm::mat4> &transforms) = 0;
        // virtual void setCamera(const Camera &camera) = 0;
        // virtual void setScene(const Scene &scene) = 0;
        // virtual void renderScene(const Scene &scene, const Camera &camera,
        //                 const std::filesystem::path &outputPath) = 0;
        virtual bool initialize(int deviceIndex) = 0;
        virtual bool prepareRenderingPipeline() = 0;

        // virtual void render(const Camera& camera) = 0;
        // virtual void getPixels(std::vector<glm::vec4>& pixels) = 0;
    };
}


