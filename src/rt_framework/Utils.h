#pragma once

#include "RenderSessionTypes.h"
#include <print>
#include <array>
#include <fstream>
#include <string>

void optixLogCallback(unsigned int level, const char* tag, const char* message, void*);

void writeOptixMatrix(const glm::mat4& m, float (&out)[12]);

std::filesystem::path resolveDevicePath(rtf::RenderBackend backend, std::string filename);

std::string readFile(const std::filesystem::path& path);