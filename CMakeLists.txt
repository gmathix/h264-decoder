cmake_minimum_required(VERSION 3.20)
project(undo264
        VERSION 1.0
        DESCRIPTION "A H.264 decoder written in C"
        LANGUAGES C
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# default to a sane build type if none is given (for makefile/ninja only)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo)
endif()

option(USE_NATIVE_ARCH "Optimize for the host CPU (-march:native); breaks portability" OFF)
option(USE_SANITIZERS "Build with ASan/UBSan" OFF)
option(BUILD_TOOLS "Build gen_rgb_video / compare_streams helper tools" ON)

set(UNDO264_SOURCES
    src/main.c
    src/intra.c
    src/annexb.c
    src/ps.c
    src/decoder.c
    src/mb.c
    src/cavlc.c
    src/vlc.c
    src/picture.c
    src/dequant.c
    src/dpb.c
    src/deblock.c
    src/cabac.c
        src/util/logger.c
    src/util/expgolomb.c
    src/tests/profiler.c
        src/dsp_init.c
        src/dsp_init.h
)

# warning/flag interface
add_library(undo264_warnings INTERFACE)
target_compile_options(undo264_warnings INTERFACE
    $<$<C_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<AND:$<C_COMPILER_ID:GNU,Clang>,$<BOOL:${USE_NATIVE_ARCH}>>:-march=native>
    $<$<AND:$<C_COMPILER_ID:GNU,Clang>,$<CONFIG:Release,Profile>>:-O3>
    $<$<AND:$<C_COMPILER_ID:GNU,Clang>,$<CONFIG:Debug>>:-O0 -g3>
)

add_library(undo264_sanitizers INTERFACE)
if(USE_SANITIZERS)
    target_compile_options(undo264_sanitizers INTERFACE -fsanitize=address,undefined -g3 -O0)
    target_link_options(undo264_sanitizers INTERFACE -fsanitize=address,undefined)
endif()


# main target
add_executable(undo264 ${UNDO264_SOURCES})
target_link_libraries(undo264 PRIVATE m undo264_warnings undo264_sanitizers)
target_include_directories(undo264 PRIVATE src)
target_compile_definitions(undo264 PRIVATE _DEFAULT_SOURCE
    $<$<CONFIG:Debug>:SLICES_LOG=1>
)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    target_compile_definitions(undo264 PRIVATE ARCH_X86=1)
endif()


# optional helper tools
if(BUILD_TOOLS)
    add_executable(gen_rgb_video src/tests/gen_rgb_video.c)
    target_link_libraries(gen_rgb_video PRIVATE m)

    add_executable(compare_streams src/tests/streamcomp.c)
endif()


# install rules
include(GNUInstallDirs)
install(TARGETS undo264 RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})