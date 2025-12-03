#include "../RendererFactories.h"
#include "../RenderSession.h"

#if defined(USE_CUDA)
#include "OptixRenderer.h"

namespace rtf {
std::unique_ptr<Renderer> createOptixRenderer(RenderSession* session) {
    return std::make_unique<OptixRenderer>(session);
}
}
#endif
