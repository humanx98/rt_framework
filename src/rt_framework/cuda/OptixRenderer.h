#pragma once

#include "../Renderer.h"

namespace rtf {
    class OptixRenderer : public Renderer {
    public:
        ~OptixRenderer();
        virtual void init(int deviceIndex, glm::uvec2 resolution, const Scene& scene) override;
        virtual void render(const Camera& camera) override;
        virtual void getPixels(std::vector<glm::vec4>& out) override;
    };
}
