#pragma once

#include <hiprt/hiprt.h>
#include <hip/hip_runtime.h>
#include <print>
#include <stdexcept>
#include <utility>

namespace rtf {
    static void hipCheck(hipError_t err, const char * file, const char * func, const int line, const char* expression) {
        if (err != hipSuccess) {
            std::println("{}:{} in {}", file, line, func);
            std::println("\t {} returned {}", expression, (int)err);
            throw std::runtime_error("HIP returned an error.");
        }
    }

    static void hiprtCheck(hiprtError err, const char * file, const char * func, const int line, const char* expression) {
        if (err != hiprtSuccess) {
            std::println("{}:{} in {}", file, line, func);
            std::println("\t {} returned {}", expression, (int)err);
            throw std::runtime_error("HIPRT returned an error.");
        }
    }
}

#define HIP_CHECK(err) rtf::hipCheck(err, __FILE__, __func__, __LINE__, #err)
#define HIPRT_CHECK(err) rtf::hiprtCheck(err, __FILE__, __func__, __LINE__, #err)

#if defined(__clang__) || defined(__GNUC__)
    #define DISABLE_DEPRECATED_WARNINGS \
        _Pragma("GCC diagnostic push") \
        _Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"")

    #define ENABLE_DEPRECATED_WARNINGS \
        _Pragma("GCC diagnostic pop")

#elif defined(_MSC_VER)
    #define DISABLE_DEPRECATED_WARNINGS \
        __pragma(warning(push)) \
        __pragma(warning(disable:4996))

    #define ENABLE_DEPRECATED_WARNINGS \
        __pragma(warning(pop))

#else
    #define DISABLE_DEPRECATED_WARNINGS
    #define ENABLE_DEPRECATED_WARNINGS
#endif
