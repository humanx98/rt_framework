#include "OptixRenderer.h"
#include <print>

namespace rtf {
    OptixRenderer::~OptixRenderer() {
        std::println("~optix");
    }

    void OptixRenderer::init(int deviceIndex, glm::uvec2 resolution, const Scene& scene) {
        std::println("optix init deviceIndex = {}", deviceIndex);
    }

    void OptixRenderer::render(const Camera& camera) {
        std::println("optix render");
    }

    void OptixRenderer::getPixels(std::vector<glm::vec4>& out) {
        std::println("optix getPixels()");
    }
}
