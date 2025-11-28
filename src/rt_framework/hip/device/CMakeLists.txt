
# find_file(HIP_LINKER_EXECUTABLE amdclang++ clang++.exe HINTS ${ROCM_PATH} PATH_SUFFIXES bin REQUIRED)
set(HIP_LINKER_EXECUTABLE ${HIP_HIPCC_EXECUTABLE})

# set(hip_arch
#     gfx1100 gfx1101 gfx1102 gfx1103 gfx1150 gfx1151  # Navi3
#     gfx1030 gfx1031 gfx1032 gfx1033 gfx1034 gfx1035 gfx1036  # Navi2
#     gfx1010 gfx1011 gfx1012 gfx1013  # Navi1
#     gfx900 gfx902 gfx904 gfx906 gfx908 gfx909 gfx90a gfx90c gfx940 gfx941 gfx942
#     CACHE STRING "AMD HIP architectures to build binaries for")
set(HIP_ARCH gfx1031 CACHE STRING "AMD HIP architectures to build binaries for")
set(HIP_DEVICE_MODULES
    RenderNormalsPrimitives.hip.cpp
)

set(hip_target_gpus)
foreach(arch ${HIP_ARCH})
    list(APPEND hip_target_gpus "--offload-arch=${arch}")
endforeach()
set(hip_device_compile_options
    -O3
    -std=c++17
    ${hip_target_gpus}
    -fgpu-rdc
    -c
    --gpu-bundle-output
    -c
    -emit-llvm
    -I ${HIPRT_INCLUDE_DIR}/
    -I ${CMAKE_CURRENT_SOURCE_DIR}
    -ffast-math
    -parallel-jobs=15
)
set(hip_device_link_options
    -fgpu-rdc
    --hip-link
    --cuda-device-only
    ${hip_target_gpus}
    ${HIPRT_BITCODE}
)
set(hip_compiled_modules)
file(GLOB hip_module_headers "*.hip.h")
foreach(hip_module_source ${HIP_DEVICE_MODULES})
    get_filename_component(hip_module_name "${hip_module_source}" NAME_WE)
    add_custom_command(
        DEPENDS ${hip_module_source} ${hip_module_headers}
        OUTPUT  ${hip_module_name}.hipfb
        COMMAND ${HIP_HIPCC_EXECUTABLE} ${hip_device_compile_options} ${CMAKE_CURRENT_SOURCE_DIR}/${hip_module_source} -o ${hip_module_name}.bc
        COMMAND ${HIP_LINKER_EXECUTABLE} ${hip_device_link_options} ${hip_module_name}.bc -o ${hip_module_name}.hipfb
        COMMENT "Compiling ${hip_module_name} for ${HIP_ARCH}"
    )
    list(APPEND hip_compiled_modules ${hip_module_name}.hipfb)
endforeach()

add_custom_target(
    hip_device_modules
    DEPENDS ${hip_compiled_modules}
)
