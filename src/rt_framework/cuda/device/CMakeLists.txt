set(OPTIX_COMPUTE_CAPABILITY "75" CACHE STRING "Compute capability (e.g. 75, 80, 86, 90) used when generating OptiX PTX")

if (NOT CUDAToolkit_FOUND)
    message(FATAL_ERROR "CUDAToolkit must be found before building OptiX device modules")
endif()

if (NOT OPTIX_INCLUDE_DIR)
    message(FATAL_ERROR "OPTIX_INCLUDE_DIR is not defined")
endif()

set(OPTIX_DEVICE_SOURCES
    OptixMotionBlur.cu
)

set(optix_ptx_outputs)
foreach(src ${OPTIX_DEVICE_SOURCES})
    get_filename_component(src_name "${src}" NAME_WE)
    set(output_ptx "${CMAKE_CURRENT_BINARY_DIR}/${src_name}.ptx")
    message(STATUS "Generating PTX for ${src} at compute capability ${OPTIX_COMPUTE_CAPABILITY}")
    add_custom_command(
        OUTPUT ${output_ptx}
        COMMAND ${CUDAToolkit_NVCC_EXECUTABLE}
            --std=c++17
            --use_fast_math
            --gpu-architecture=compute_${OPTIX_COMPUTE_CAPABILITY}
            -I ${OPTIX_INCLUDE_DIR}
            -I ${CMAKE_CURRENT_SOURCE_DIR}/..
            -ptx ${CMAKE_CURRENT_SOURCE_DIR}/${src}
            -o ${output_ptx}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${src} ${CMAKE_CURRENT_SOURCE_DIR}/../OptixLaunchParams.h
        COMMENT "Compiling ${src} to PTX"
    )
    list(APPEND optix_ptx_outputs ${output_ptx})
endforeach()

add_custom_target(
    optix_device_ptx
    DEPENDS ${optix_ptx_outputs}
)
