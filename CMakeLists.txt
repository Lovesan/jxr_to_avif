cmake_minimum_required(VERSION 3.23)
project(jxr_to_avif C CXX)
include(CheckCXXCompilerFlag)

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 17)


if(NOT EXISTS "${PROJECT_SOURCE_DIR}/libavif/CMakeLists.txt")
    message(FATAL_ERROR "The libavif submodule was not downloaded! Please update submodules and try again.")
endif()
if(NOT EXISTS "${PROJECT_SOURCE_DIR}/simd_math/simd_math.h")
    message(FATAL_ERROR "The simd_math submodule was not downloaded! Please update submodules and try again.")
endif()

if(MSVC)
    add_definitions(/arch:AVX2)
elseif(NOT MSVC)
    set(CMAKE_C_FLAGS "-mavx2 -mfma -march=native")
    set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS}")
endif()

set(ENABLE_DOCS 0)
set(ENABLE_EXAMPLES 0)
set(ENABLE_TESTDATA 0)
set(ENABLE_TESTS 0)
set(ENABLE_TOOLS 0)
set(CONFIG_PIC 1)
set(ENABLE_SHARED OFF)
set(ENABLE_STATIC ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(BUILD_SHARED_LIBS OFF)
set(AVIF_CODEC_AOM LOCAL)
set(AVIF_LIBYUV LOCAL)
set(AVIF_BUILD_APPS OFF)
add_subdirectory(libavif)

if(MSVC)
    add_definitions(/W4)
elseif(NOT MSVC)
    add_compile_options(-Wall -Wextra -ffast-math)
    add_link_options(-s --static)
endif()

include_directories(simd_math)

add_executable(jxr_to_avif main.cxx jxr_data.c jxr_data.h CommandLineParser.hpp CommandLineParser.cxx PixelFormat.hpp
                           jxr_sys_helpers.h jxr_sys_helpers.c)

target_link_libraries(jxr_to_avif avif aom uuid windowscodecs)

install(TARGETS jxr_to_avif)
