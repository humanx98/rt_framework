#pragma once
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <stdexcept>
#include <vector>
#include <glm/ext/matrix_transform.hpp>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image.h>
#include <stb_image_write.h>

#include <memory>
#include <print>
#include <filesystem>
#include <numbers>
#include "Renderer.h"
#include "MeshType.h"

namespace rtf {
std::vector<glm::u8vec4> Convertu8vec4(const std::vector<glm::vec4>& data) {
    std::vector<glm::u8vec4> result;
    result.reserve(data.size());
    for (const auto& p : data) {
        glm::vec4 clamped = glm::clamp(p * 255.0f, 0.0f, 255.0f);
        result.push_back(glm::u8vec4(
            static_cast<uint8_t>(clamped.r),
            static_cast<uint8_t>(clamped.g),
            static_cast<uint8_t>(clamped.b),
            static_cast<uint8_t>(clamped.a)
        ));
    }
    return result;
}

bool SaveImage(const std::filesystem::path& path, int width, int height, const std::vector<glm::vec4>& data) {
    if (data.empty()) return false;
    std::vector<glm::u8vec4> convertedData = Convertu8vec4(data);
    int success = stbi_write_png(path.string().c_str(), width, height, 4, convertedData.data(), width * 4 * sizeof(uint8_t));
    return success != 0;
}

bool LoadTriangle(rtf::Scene& scene) {
    std::vector<glm::uvec3> triangles = { glm::uvec3(0, 1, 2) };
    constexpr float scale = 0.15f;
    std::vector<glm::vec3> vertices = {
        glm::vec3(scale * sinf( 0.0f ), scale * cosf( 0.0f ), 0.0f ),
        glm::vec3(scale * sinf( std::numbers::pi_v<float> * 2.0f / 3.0f ), scale * cosf( std::numbers::pi_v<float> * 2.0f / 3.0f ), 0.0f ),
        glm::vec3(scale * sinf( std::numbers::pi_v<float> * 4.0f / 3.0f ), scale * cosf( std::numbers::pi_v<float> * 4.0f / 3.0f ), 0.0f )
    };
    scene.meshes.emplace_back(std::move(vertices), std::move(triangles));
    return true;
}

bool LoadInstanceData(rtf::Scene& scene)
{
    constexpr float offset = 0.3f;

    auto makeInstance = [&](std::vector<glm::mat4> matrices) {
        const size_t keyCount = matrices.size();
        std::vector<rtf::Transform> transforms;
        transforms.reserve(keyCount);
        for (size_t i = 0; i < keyCount; ++i) {
            float t = keyCount > 1 ? static_cast<float>(i) / static_cast<float>(keyCount - 1) : 0.0f;
            transforms.push_back({ .matrix = matrices[i], .time = t });
        }
        scene.instances.emplace_back(static_cast<glm::uint>(scene.meshes.size() - 1), std::move(transforms));
    };

    makeInstance({
        glm::translate(glm::mat4(1.0f), { -0.25f, -offset, 0.0f }),
        glm::translate(glm::mat4(1.0f), { 0.0f, -offset, 0.0f }),
        glm::translate(glm::mat4(1.0f), { 0.25f, -offset, 0.0f }) *
            glm::rotate(glm::mat4(1.0f), std::numbers::pi_v<float> * 0.5f, { 0.0f, 0.0f, 1.0f })
    });

    makeInstance({
        glm::translate(glm::mat4(1.0f), { 0.0f, offset, 0.0f }),
        glm::translate(glm::mat4(1.0f), { 0.0f, offset, 0.0f }) *
            glm::rotate(glm::mat4(1.0f), std::numbers::pi_v<float> * 0.5f, { 0.0f, 0.0f, 1.0f })
    });

    return true;
}

bool LoadMesh(MeshType type, rtf::Scene& scene)
{
    rtf::Camera camera;
    switch (type) {
    case MeshType::Triangle:
        LoadTriangle(scene);
        LoadInstanceData(scene);
        return true;
    default:
        std::println("Unsupported mesh type");
        return false;
    }
}



bool SetupCameraFromMesh(const rtf::Scene& scene, rtf::Camera& camera, const float lookFromFactor)
{
    // Compute min bounding box:
    glm::vec3 minBB( std::numeric_limits<float>::max() );
    glm::vec3 maxBB( std::numeric_limits<float>::lowest() );

    for (const auto& mesh : scene.meshes) {
        for (const auto& vertex : mesh.vertices) {
            minBB = glm::min(minBB, vertex);
            maxBB = glm::max(maxBB, vertex);
        }
    }
    
    glm::vec3 center = (minBB + maxBB) * 0.5f;

    // from is 2x the max extent away from center
    glm::vec3 extent = maxBB - minBB;
    float maxExtent = 0.f;
	for(glm::length_t i = 1, n = extent.length(); i < n; ++i)
			maxExtent = glm::max(maxExtent, extent[i]);
    
    glm::vec3 lookFrom = center + glm::vec3(0.0f
        , 0.0f
        , lookFromFactor * maxExtent);
    glm::vec3 lookAt = center;
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    float fov = std::numbers::pi_v<float> / 4.0f;
    
    camera = {
        .lookFrom = lookFrom,
        .lookAt = lookAt,
        .up = up,
        .vfov = fov,
    };
    return true;
}

} // namespace rtf

struct AssimpSceneDeleter {
    void operator()(const struct aiScene* scene) { aiReleaseImport(scene); }
};

static void loadSceneWithAssimp(rtf::Scene& scene, const std::filesystem::path& path) {
    uint32_t processFlags = aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_SortByPType | aiProcess_GenSmoothNormals;
    std::unique_ptr<const struct aiScene, AssimpSceneDeleter> assimpScene(aiImportFile(path.string().c_str(), processFlags));

    if (assimpScene == NULL) {
        std::println("Failed to load scene with assimp at: {}", path.string());
        throw std::runtime_error("");
    }

    scene.meshes.reserve(assimpScene->mNumMeshes);
    scene.instances.reserve(assimpScene->mNumMeshes);
    for (int i = 0; i < assimpScene->mNumMeshes; i++) {
        const struct aiMesh* mesh = assimpScene->mMeshes[i];

        std::vector<glm::uvec3> triangles;
        triangles.reserve(mesh->mNumFaces);
        for (int j = 0; j < mesh->mNumFaces; j++) {
            const struct aiFace face = mesh->mFaces[j];
            if (face.mNumIndices != 3) {
                assert(processFlags & aiProcess_Triangulate);
                throw std::runtime_error("Only triangles are expected here");
            }

            triangles.push_back(glm::uvec3(
                face.mIndices[0],
                face.mIndices[1],
                face.mIndices[2]
            ));
        }

        static_assert(sizeof(mesh->mVertices[0]) == sizeof(glm::vec3), "Check assimp vertices type");
        std::vector<glm::vec3> vertices((glm::vec3*)mesh->mVertices, (glm::vec3*)mesh->mVertices + mesh->mNumVertices);
        scene.meshes.emplace_back(std::move(vertices), std::move(triangles));

        rtf::Transform transform = {
                .matrix = glm::mat4(1.0f),
                .time = 0.0f
        };
        scene.instances.emplace_back((glm::uint)scene.meshes.size() - 1, transform);
    }
}

static void loadLucy(rtf::Scene& scene, rtf::Camera& camera) {
    loadSceneWithAssimp(scene, "meshes/lucy.obj");
    camera = {
        .lookFrom = glm::vec3(0.0f, 1600.0f, 1500.0f),
        .lookAt = glm::vec3(0.0f, 450.0f, -300.0f),
        .up = glm::vec3(0.0f, 1.0f, 0.0f),
        .vfov = std::numbers::pi_v<float> / 9.0f,
    };
}

static void loadCornellPlot(rtf::Scene& scene, rtf::Camera& camera) {
    loadSceneWithAssimp(scene, "meshes/cornellpot.obj");
    camera = {
        .lookFrom = glm::vec3(0.0f, 2.5f, 20.0f),
        .lookAt = glm::vec3(0.0f, 2.5f, 0.0),
        .up = glm::vec3(0.0f, 1.0f, 0.0f),
        .vfov = std::numbers::pi_v<float> / 9.0f,
    };
}

static void loadMotionBlur(rtf::Scene& scene, rtf::Camera& camera) {
    camera= {
        .lookFrom = glm::vec3(0.0f, 0.0f, -1.0),
        .lookAt = glm::vec3(0.0f, 0.0f, 0.0),
        .up = glm::vec3(0.0f, 1.0f, 0.0f),
        .vfov = std::numbers::pi_v<float> / 4.0f,
    };
    std::vector<glm::uvec3> triangles = { glm::uvec3(0, 1, 2) };
    constexpr float scale = 0.15f;
    std::vector<glm::vec3> vertices = {
        glm::vec3(scale * sinf( 0.0f ), scale * cosf( 0.0f ), 0.0f ),
        glm::vec3(scale * sinf( std::numbers::pi_v<float> * 2.0f / 3.0f ), scale * cosf( std::numbers::pi_v<float> * 2.0f / 3.0f ), 0.0f ),
        glm::vec3(scale * sinf( std::numbers::pi_v<float> * 4.0f / 3.0f ), scale * cosf( std::numbers::pi_v<float> * 4.0f / 3.0f ), 0.0f )
    };
    scene.meshes.emplace_back(std::move(vertices), std::move(triangles));

    // instance 0
    constexpr float offset = 0.3f;
    {
        std::vector<rtf::Transform> transforms = {
            {
                .matrix = glm::translate(glm::mat4(1.0f), { -0.25f, -offset, 0.0f }),
                .time = 0.0f
            }
            ,
            {
                .matrix = glm::translate(glm::mat4(1.0f), { 0.0f, -offset, 0.0f }),
                .time = 0.35f
            },
            {
                .matrix = glm::translate(glm::mat4(1.0f), { 0.25f, -offset, 0.0f })
                    * glm::rotate(glm::mat4(1.0f), std::numbers::pi_v<float> * 0.25f, { 0.0f, 0.0f, 1.0f }),
                .time = 1.0f
            }
        };
        scene.instances.emplace_back((glm::uint)scene.meshes.size() - 1, std::move(transforms));
    }
    // instance 1
    {
        std::vector<rtf::Transform> transforms = {
            {
                .matrix = glm::translate(glm::mat4(1.0f), { 0.0f, offset, 0.0f }),
                .time = 0.0f
            }
            ,
            {
                .matrix = glm::translate(glm::mat4(1.0f), { 0.0f, offset, 0.0f })
                    * glm::rotate(glm::mat4(1.0f), std::numbers::pi_v<float> * 0.5f, { 0.0f, 0.0f, 1.0f }),
                .time = 1.0f
            }
        };
        scene.instances.emplace_back((glm::uint)scene.meshes.size() - 1, std::move(transforms));
    }
}

// static void render(rtf::Renderer& renderer, const std::filesystem::path& png) {
//     const int deviceIdnex = 0;
//     glm::uvec2 resolution(1920, 1080);
//     rtf::Camera camera;
//     rtf::Scene scene;
//     loadMotionBlur(scene, camera);

//     renderer.init(deviceIdnex, resolution, scene);
//     renderer.render(camera);

//     std::vector<glm::vec4> rgba32f;
//     renderer.getPixels(rgba32f);
//     if (!rgba32f.empty()) {
//         std::vector<glm::u8vec4> rgba8;
//         rgba8.reserve(rgba32f.size());
//         for (auto p : rgba32f) {
//             p = p * 255.0f;
//             rgba8.push_back({
//                 (glm::u8)std::clamp(p.r, 0.0f, 255.0f),
//                 (glm::u8)std::clamp(p.g, 0.0f, 255.0f),
//                 (glm::u8)std::clamp(p.b, 0.0f, 255.0f),
//                 (glm::u8)std::clamp(p.a, 0.0f, 255.0f)
//             });
//         }

//         stbi_write_png(png.string().c_str(), resolution.x, resolution.y, 4, rgba8.data(), resolution.x * sizeof(rgba8[0]));
//         std::println("Result stored at: {}", png.string());
//     } else {
//         std::println("No result");
//     }
// }