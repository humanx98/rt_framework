#pragma once

#include <vector>
#include <glm/glm.hpp>

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

    struct TriangleMesh : public OnlyMovable {
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

    struct Instance : public OnlyMovable {
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

    struct Scene : public OnlyMovable {
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
        virtual void init(int deviceIndex, glm::uvec2 resolution, const Scene& scene) = 0;
        virtual void render(const Camera& camera) = 0;
        virtual void getPixels(std::vector<glm::vec4>& pixels) = 0;
    };
}
