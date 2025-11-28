if (NOT DEFINED HIP_MAJOR_MINOR_VERSION)
    message(FATAL_ERROR "HIP_MAJOR_MINOR_VERSION is not defined")
endif()

set(HIPRT_DIR ${CMAKE_SOURCE_DIR}/submodules/HIPRT)
set(HIPRT_MAJOR_VERSION 3)
set(HIPRT_MINOR_VERSION 0)
set(HIPRT_VERSION "${HIPRT_MAJOR_VERSION}.${HIPRT_MINOR_VERSION}")
set(HIPRT_VERSION_PREFIX "0${HIPRT_MAJOR_VERSION}00${HIPRT_MINOR_VERSION}")

set(HIPRT_LIBRARY_DIR ${HIPRT_DIR}/dist/bin/Release)
set(HIPRT_PLATFORM)
if(UNIX)
    set(HIPRT_PLATFORM linux)
else()
    set(HIPRT_PLATFORM win)
endif()

find_path(HIPRT_INCLUDE_DIR NAMES hiprt/hiprt.h PATHS ${HIPRT_DIR})
find_library(HIPRT_LIBRARY hiprt${HIPRT_VERSION_PREFIX}64 HINTS ${HIPRT_LIBRARY_DIR} REQUIRED)
find_file(HIPRT_BITCODE hiprt${HIPRT_VERSION_PREFIX}_${HIP_MAJOR_MINOR_VERSION}_amd_lib_${HIPRT_PLATFORM}.bc HINTS ${HIPRT_LIBRARY_DIR} REQUIRED)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(HIPRT
    REQUIRED_VARS HIPRT_LIBRARY HIPRT_INCLUDE_DIR HIPRT_BITCODE
    VERSION_VAR HIPRT_VERSION
)

if(HIPRT_FOUND)
    if(NOT TARGET HIPRT::HIPRT)
        add_library(HIPRT::include IMPORTED INTERFACE)
        target_include_directories(HIPRT::include INTERFACE ${HIPRT_INCLUDE_DIR})

        add_library(HIPRT::library IMPORTED SHARED)
        if(UNIX)
            set_target_properties(HIPRT::library PROPERTIES IMPORTED_LOCATION ${HIPRT_LIBRARY})
        else()
            find_file(HIPRT_DLL hiprt${HIPRT_VERSION_PREFIX}64.dll HINTS ${HIPRT_LIBRARY_DIR} REQUIRED)
            set_target_properties(HIPRT::library PROPERTIES IMPORTED_IMPLIB ${HIPRT_LIBRARY})
            set_target_properties(HIPRT::library PROPERTIES IMPORTED_LOCATION ${HIPRT_DLL})
        endif()

        add_library(HIPRT::HIPRT IMPORTED INTERFACE)
        target_link_libraries(HIPRT::HIPRT INTERFACE HIPRT::include HIPRT::library)
        set_target_properties(HIPRT::HIPRT PROPERTIES BITCODE ${HIPRT_BITCODE})
    endif()
endif()
