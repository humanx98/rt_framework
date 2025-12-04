#pragma once

#include "RenderSession.h"
#include "Renderer.h"
#include <memory>


namespace rtf {
#if defined(USE_HIP)
std::unique_ptr<Renderer> createHiprtRenderer(RenderSession *session);
#endif

#if defined(USE_CUDA)
std::unique_ptr<Renderer> createOptixRenderer(RenderSession *session);
#endif
} // namespace rtf
