
// #include "Renderer.h"
// #include <algorithm>
// #include <filesystem>
// #include <print>
// #include <memory>
// #include <assimp/cimport.h>
// #include <assimp/scene.h>
// #include <assimp/postprocess.h>
// #include <stdexcept>
// #include <vector>
// #include <glm/ext/matrix_transform.hpp>

// #define STB_IMAGE_IMPLEMENTATION
// #define STB_IMAGE_WRITE_IMPLEMENTATION
// #include <stb_image.h>
// #include <stb_image_write.h>

// #include "RenderSession.h"

// #ifdef USE_HIP
// #include "hip/HiprtRenderer.h"
// #endif

// #ifdef USE_CUDA
// #include "cuda/OptixRenderer.h"
// #endif
// #include <numbers>

#include "AssetsLoader.h"
#include "CommandLine.h"
#include "RenderSession.h"

int main(int argc, const char *argv[]) {
  const rtf::CliParseResult cli = rtf::ParseCommandLine(argc, argv);
  if (cli.mode == rtf::CliRunMode::ExitSuccess) {
    return EXIT_SUCCESS;
  }
  if (cli.mode == rtf::CliRunMode::ExitFailure) {
    return EXIT_FAILURE;
  }
  const rtf::CommandLineOptions &options = cli.options;

  // Create mesh and camera
  rtf::Scene scene;
  rtf::Camera camera;

  if (!LoadMesh(options.meshType, scene)) {
    std::println("Failed to load requested mesh");
    return EXIT_FAILURE;
  }
  SetupCameraFromMesh(scene, camera, 6.7f);

  // print camera parmeters:
  std::println("Camera:");
  std::println("  lookFrom: ({}, {}, {})", camera.lookFrom.x, camera.lookFrom.y,
               camera.lookFrom.z);
  std::println("  lookAt:   ({}, {}, {})", camera.lookAt.x, camera.lookAt.y,
               camera.lookAt.z);
  std::println("  up:       ({}, {}, {})", camera.up.x, camera.up.y,
               camera.up.z);
  std::println("  vfov:     {}", camera.vfov);

  rtf::RenderSessionOptions sessionOptions{.backend = options.backend,
                                           .enableMotionBlur =
                                               options.enableMotionBlur,
                                           .outputPath = options.outputPath,
                                           .scene = scene,
                                           .camera = camera};

  std::unique_ptr<rtf::RenderSession> session;
  try {
    session = rtf::RenderSession::create();
  } catch (const std::runtime_error &e) {
    std::println("Failed to create RenderSession: {}", e.what());
    return EXIT_FAILURE;
  }

  session->initialize(sessionOptions);
  session->prepareRenderingPipeline();
  session->renderFrame();
 
  std::vector<glm::vec4> frameData;
  session->getFrameData(frameData);


  rtf::SaveImage(sessionOptions.outputPath, sessionOptions.resolution.x, sessionOptions.resolution.y, frameData);
  return EXIT_SUCCESS;
}