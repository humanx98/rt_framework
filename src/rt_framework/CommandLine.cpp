#include "CommandLine.h"

#include <algorithm>
#include <filesystem>
#include <optional>
#include <print>
#include <string_view>
#include <string>
#include <vector>

#include "RenderSession.h"

namespace rtf {
namespace {

struct BackendInfo {
    std::string_view name;
    RenderBackend backend;
    std::string_view description;
};

struct MeshInfo {
    std::string_view name;
    MeshType type;
    std::string_view description;
};

struct OptionInfo {
    std::string_view flag;
    std::string_view description;
};

constexpr MeshInfo kMeshTypes[] = {
    {"triangle", MeshType::Triangle, "Built-in triangle primitive"},
    {"file", MeshType::File, "Placeholder for external mesh file"}
};

constexpr OptionInfo kOptionInfos[] = {
    {"--help", "Show usage information"},
    {"--list-backends", "List compiled backends"},
    {"--list-meshes", "List supported mesh presets"},
    {"--list-options", "List every command line flag"},
    {"--backend=<name>", "Select rendering backend (hiprt, optix)"},
    {"--mesh-type=<name>", "Select mesh preset (triangle, file)"},
    {"--motion", "Enable motion blur transforms"},
    {"--output=<path>", "Output image file path"}
};

std::optional<std::string_view> ExtractValue(std::string_view arg, std::string_view prefix) {
    if (arg.rfind(prefix, 0) == 0) {
        return arg.substr(prefix.size());
    }
    return std::nullopt;
}

std::vector<BackendInfo> GatherAvailableBackends() {
    std::vector<BackendInfo> infos;
#ifdef USE_HIP
    infos.push_back({"hiprt", RenderBackend::Hiprt, "HIPRT backend"});
#endif
#ifdef USE_CUDA
    infos.push_back({"optix", RenderBackend::Optix, "OptiX backend"});
#endif
    return infos;
}

void PrintUsageInternal() {
    std::println("Usage: <executable> [--backend=<name>] [options] <backend?> [output.png]");
    std::println("Specify the backend either as the first positional argument or via --backend.");
    std::println("");
    std::println("Examples:");
    std::println("  app --backend=hiprt --mesh-type=triangle result.png");
    std::println("  app --backend=optix --motion --output frame.png");
}

void PrintAvailableBackendsInternal() {
    auto infos = GatherAvailableBackends();
    if (infos.empty()) {
        std::println("No rendering backends compiled. Enable USE_HIP or USE_CUDA.");
        return;
    }

    std::println("Available backends:");
    for (const auto& backend : infos) {
        std::println("  {:<12}{}", backend.name, backend.description);
    }
}

void PrintAvailableMeshTypesInternal() {
    std::println("Mesh presets:");
    for (const auto& mesh : kMeshTypes) {
        std::println("  {:<12}{}", mesh.name, mesh.description);
    }
}

void PrintOptionListInternal() {
    std::println("Options:");
    for (const auto& option : kOptionInfos) {
        std::println("  {:<18}{}", option.flag, option.description);
    }
}

std::optional<MeshType> ParseMesh(std::string_view name) {
    auto it = std::find_if(std::begin(kMeshTypes), std::end(kMeshTypes), [&](const MeshInfo& info) {
        return info.name == name;
    });
    if (it == std::end(kMeshTypes)) {
        return std::nullopt;
    }
    return it->type;
}

std::optional<RenderBackend> ParseBackend(std::string_view name) {
    auto infos = GatherAvailableBackends();
    auto it = std::find_if(infos.begin(), infos.end(), [&](const BackendInfo& backend) {
        return backend.name == name;
    });
    if (it == infos.end()) {
        return std::nullopt;
    }
    return it->backend;
}

bool AssignBackend(std::string_view name, CommandLineOptions& options,
                   std::string& backendLabel) {
    auto backend = ParseBackend(name);
    if (!backend) {
        std::println("Unknown backend: {}", name);
        PrintAvailableBackendsInternal();
        return false;
    }
    options.backend = *backend;
    backendLabel = std::string(name);
    return true;
}

bool HandleInfoFlag(std::string_view arg) {
    if (arg == "--help" || arg == "-h") {
        PrintUsageInternal();
        std::println("");
        PrintAvailableBackendsInternal();
        std::println("");
        PrintAvailableMeshTypesInternal();
        std::println("");
        PrintOptionListInternal();
        return true;
    }
    if (arg == "--list-backends") {
        PrintAvailableBackendsInternal();
        return true;
    }
    if (arg == "--list-meshes") {
        PrintAvailableMeshTypesInternal();
        return true;
    }
    if (arg == "--list-options") {
        PrintOptionListInternal();
        return true;
    }
    return false;
}

} // namespace

CliParseResult ParseCommandLine(int argc, const char* argv[]) {
    CliParseResult result{};
    result.options.backend = RenderBackend::None;
    result.options.meshType = MeshType::Triangle;
    std::string backendLabel;

    if (argc < 2) {
        PrintUsageInternal();
        return {.options = result.options, .mode = CliRunMode::ExitFailure};
    }

    for (int i = 1; i < argc; ++i) {
        if (HandleInfoFlag(argv[i])) {
            return {.options = result.options, .mode = CliRunMode::ExitSuccess};
        }
    }

    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];

        if (auto value = ExtractValue(arg, "--backend=")) {
            if (!AssignBackend(*value, result.options, backendLabel)) {
                return {.options = result.options, .mode = CliRunMode::ExitFailure};
            }
            continue;
        }

        if (arg == "--backend") {
            if (i + 1 >= argc) {
                std::println("Missing backend name after --backend");
                return {.options = result.options, .mode = CliRunMode::ExitFailure};
            }
            if (!AssignBackend(argv[++i], result.options, backendLabel)) {
                return {.options = result.options, .mode = CliRunMode::ExitFailure};
            }
            continue;
        }

        if (arg == "--motion") {
            result.options.enableMotionBlur = true;
            continue;
        }

        if (auto value = ExtractValue(arg, "--mesh-type=")) {
            if (auto mesh = ParseMesh(*value)) {
                result.options.meshType = *mesh;
            } else {
                std::println("Unknown mesh type: {}", *value);
                PrintAvailableMeshTypesInternal();
                return {.options = result.options, .mode = CliRunMode::ExitFailure};
            }
            continue;
        }

        if (arg == "--mesh-type" || arg == "--mesh") {
            if (i + 1 >= argc) {
                std::println("Missing mesh type after {}", arg);
                return {.options = result.options, .mode = CliRunMode::ExitFailure};
            }
            ++i;
            std::string_view value = argv[i];
            if (auto mesh = ParseMesh(value)) {
                result.options.meshType = *mesh;
            } else {
                std::println("Unknown mesh type: {}", value);
                PrintAvailableMeshTypesInternal();
                return {.options = result.options, .mode = CliRunMode::ExitFailure};
            }
            continue;
        }

        if (auto value = ExtractValue(arg, "--output=")) {
            result.options.outputPath = std::filesystem::path(*value);
            continue;
        }

        if (arg == "--output") {
            if (i + 1 >= argc) {
                std::println("Missing output path after --output");
                return {.options = result.options, .mode = CliRunMode::ExitFailure};
            }
            result.options.outputPath = std::filesystem::path(argv[++i]);
            continue;
        }

        if (arg.starts_with("--")) {
            std::println("Unknown option: {}", arg);
            PrintOptionListInternal();
            return {.options = result.options, .mode = CliRunMode::ExitFailure};
        }

        if (result.options.backend == RenderBackend::None) {
            if (!AssignBackend(arg, result.options, backendLabel)) {
                return {.options = result.options, .mode = CliRunMode::ExitFailure};
            }
        } else {
            // Treat subsequent non-option tokens as output path overrides.
            result.options.outputPath = std::filesystem::path(arg);
        }
    }

    if (result.options.backend == RenderBackend::None) {
        std::println("No backend specified. Provide one as positional argument or via --backend=<name>.");
        PrintAvailableBackendsInternal();
        return {.options = result.options, .mode = CliRunMode::ExitFailure};
    }

    if (backendLabel.empty()) {
        backendLabel = "unknown";
    }

    std::println("Backend: {}", backendLabel);
    std::println("Mesh type: {}", [&]() {
        for (const auto& mesh : kMeshTypes) {
            if (mesh.type == result.options.meshType) {
                return std::string(mesh.name);
            }
        }
        return std::string("unknown");
    }());
    std::println("Motion blur: {}", result.options.enableMotionBlur ? "enabled" : "disabled");
    std::println("Output path: {}", result.options.outputPath.string());
    std::println("");

    return result;
}

} // namespace rtf
