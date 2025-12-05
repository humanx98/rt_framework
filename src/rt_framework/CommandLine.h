#pragma once

#include <filesystem>
#include <string>

#include "MeshType.h"

namespace rtf {

enum class RenderBackend;

struct CommandLineOptions {
    RenderBackend backend{};
    MeshType meshType{};
    bool enableMotionBlur{false};
    std::filesystem::path outputPath{"result.png"};
};

enum class CliRunMode { Run, ExitSuccess, ExitFailure };

struct CliParseResult {
    CommandLineOptions options{};
    CliRunMode mode{CliRunMode::Run};
};

CliParseResult ParseCommandLine(int argc, const char* argv[]);

} // namespace rtf
