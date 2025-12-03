#include "../RendererFactories.h"
#include "../RenderSession.h"

#if defined(USE_HIP)
#include "HiprtRenderer.h"
    
namespace rtf {
std::unique_ptr<Renderer> createHiprtRenderer(RenderSession* session) {
    return std::make_unique<HiprtRenderer>(session);
}
}
#endif
