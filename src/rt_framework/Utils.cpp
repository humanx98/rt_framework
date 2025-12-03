#include "Utils.h"


#include <print>
#include <array>
#include <fstream>

void optixLogCallback(unsigned int level, const char* tag, const char* message, void*) {
    std::println("[optix][{}][{}] {}", level, tag ? tag : "", message ? message : "");
}


void writeOptixMatrix(const glm::mat4& m, float (&out)[12]) {
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 4; ++col) {
            out[row * 4 + col] = m[col][row];
        }
    }
}

std::filesystem::path resolveDevicePath(rtf::RenderBackend backend, std::string filename) {
    static const std::array<const char*, 3> optixCandidates = {
        "../cuda/device",
        "cuda/device",
        "../../src/rt_framework/cuda/device"
    };

    static const std::array<const char*, 3> hiprtCandidates = {
        "../hip/device",
        "hip/device",
        "../../src/rt_framework/hip/device"
    };

    const auto& candidates = (backend == rtf::RenderBackend::Optix) ? optixCandidates : hiprtCandidates;

    for (const auto* candidate : candidates) {
        std::filesystem::path dir(candidate);
        std::filesystem::path fullPath = dir / filename;
        if (std::filesystem::exists(fullPath)) {
            return fullPath;
        }
    }

    throw std::runtime_error("Failed to locate device module.");
}



std::string readFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Unable to open PTX file at " + path.string());
    }

    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}