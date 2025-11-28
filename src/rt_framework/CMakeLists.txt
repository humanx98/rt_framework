set(USE_HIP TRUE)
set(USE_CUDA TRUE)
set(DEPENDENCIES)
set(DEFINITIONS)

set(HEADERS
    Renderer.h
)
set(SOURCES
    main.cpp
)

if(NOT DEFINED ROCM_PATH)
    set(ROCM_PATH "$ENV{HIP_PATH}" CACHE STRING "Default ROCM installation directory.")
    list(APPEND CMAKE_PREFIX_PATH ${ROCM_PATH})
endif()

find_package(hip REQUIRED)
if (HIP_HIPCC_EXECUTABLE)
    string(REGEX MATCH "([0-9]+\\.[0-9]+)" HIP_MAJOR_MINOR_VERSION ${hip_VERSION})
    find_package(HIPRT REQUIRED)

    add_subdirectory(hip/device)

    list(APPEND HEADERS
        hip/device/camera.hip.h
        hip/HipMisc.h
        hip/HiprtRenderer.h
    )

    list(APPEND SOURCES
        hip/HiprtRenderer.cpp
    )

    list(APPEND TARGETS hip::host HIPRT::HIPRT)
    list(APPEND DEPENDENCIES hip_device_modules)
    list(APPEND DEFINITIONS USE_HIP)
else()
    message(WARNING "Couldn't find hipcc. HIP is disabled")
    set(USE_HIP FALSE)
endif()

# TODO: check if cuda_found
if (TRUE)
    message(WARNING "TODO: find cuda, optix and link with them")
    list(APPEND HEADERS
        cuda/OptixRenderer.h
    )
    list(APPEND SOURCES
        cuda/OptixRenderer.cpp
    )
    # list(APPEND TARGETS hip_device_modules)
    list(APPEND DEFINITIONS USE_CUDA)
else()
    message(WARNING "Couldn't find nvcc. CUDA is disabled")
    set(USE_CUDA FALSE)
endif()

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG a532f5b1cf27d6a3c099437e6959cf7e398a0a67 #refs/tags/1.0.2
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(glm)

FetchContent_Declare(stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(stb)

FetchContent_Declare(assimp
    GIT_REPOSITORY https://github.com/assimp/assimp
    GIT_TAG v6.0.2
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)
set(ASSIMP_BUILD_SAMPLES OFF CACHE INTERNAL "Assimp build samples")
set(ASSIMP_BUILD_TESTS OFF CACHE INTERNAL "Assimp build tests")
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE INTERNAL "Assimp build assimp tools")
set(ASSIMP_BUILD_ZLIB ON CACHE INTERNAL "Assimp build zlib")
set(ASSIMP_NO_EXPORT ON CACHE INTERNAL "Assimp no export")
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE INTERNAL "Assimp build all importers by default")
set(ASSIMP_BUILD_OBJ_IMPORTER TRUE CACHE INTERNAL "Assimp build obj importer")
# set(ASSIMP_BUILD_FBX_IMPORTER TRUE CACHE INTERNAL "Assimp build fbx importer")
set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "Assimp build shared libs")
FetchContent_MakeAvailable(assimp)

add_custom_target(
    copy_meshes
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/meshes ${CMAKE_CURRENT_BINARY_DIR}/meshes
    COMMENT "Copying meshes to ${CMAKE_CURRENT_BINARY_DIR}"
)

add_executable(rt_framework ${HEADERS} ${SOURCES})
add_dependencies(rt_framework ${DEPENDENCIES} copy_meshes)
target_compile_definitions(rt_framework PRIVATE ${DEFINITIONS})
target_link_libraries(rt_framework PRIVATE ${TARGETS} glm::glm assimp::assimp)
target_include_directories(rt_framework PRIVATE ${stb_SOURCE_DIR})

add_custom_command(TARGET rt_framework POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:rt_framework> $<TARGET_RUNTIME_DLLS:rt_framework>
    COMMAND_EXPAND_LISTS
)
