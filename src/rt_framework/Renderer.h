#pragma once

#include <vector>
#include <glm/glm.hpp>

namespace rtf {
    struct TriangleMesh {
        std::vector<glm::vec3> vertices;
        std::vector<glm::uvec3> triangles;

        TriangleMesh(std::vector<glm::vec3>&& vertices, std::vector<glm::uvec3>&& triangles)
            : vertices(std::move(vertices))
            , triangles(std::move(triangles)) {
        }

        TriangleMesh(TriangleMesh&&) noexcept = default;
        TriangleMesh& operator=(TriangleMesh&&) noexcept = default;
        TriangleMesh(const TriangleMesh&) = delete;
        TriangleMesh& operator=(const TriangleMesh&) = delete;
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

        Instance(Instance&&) noexcept = default;
        Instance& operator=(Instance&&) noexcept = default;
        Instance(const Instance&) = delete;
        Instance& operator=(const Instance&) = delete;
    };

    struct Scene {
        std::vector<TriangleMesh> meshes;
        std::vector<Instance> instances;

        Scene() {}
        Scene(Scene&&) noexcept = default;
        Scene& operator=(Scene&&) noexcept = default;
        Scene(const Scene&) = delete;
        Scene& operator=(const Scene&) = delete;
    };

    struct Camera {
        glm::vec3 lookFrom;
        glm::vec3 lookAt;
        glm::vec3 up;
        float vfov;
    };

    class Renderer {
    public:
        Renderer() {}
        virtual ~Renderer() {}
        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;
        virtual void init(int deviceIndex, glm::uvec2 resolution, const Scene& scene) = 0;
        virtual void render(const Camera& camera) = 0;
        virtual void getPixels(std::vector<glm::vec4>& pixels) = 0;
    };
}
