cmake_minimum_required(VERSION 4.0)
project(h264_decoder C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(H264_SOURCES
        main.c

        src/intra.c
        src/annexb.c
        src/ps.c
        src/decoder.c
        src/mb.c
        src/cavlc.c
        src/vlc.c
        src/transform.c
        src/picture.c
        src/dequant.c
        src/dpb.c
        src/inter.c
        src/qpel.c
        src/deblock.c
        src/cabac.c

        src/util/bitreader.c
        src/util/logger.c
        src/util/expgolomb.c
        src/tests/profiler.c
)

add_executable(h264_decoder ${H264_SOURCES})
target_compile_options(h264_decoder PRIVATE
        -O3
        -march=native
)
target_link_libraries(h264_decoder PRIVATE m)

add_executable(h264_decoder_debug ${H264_SOURCES})
target_compile_options(h264_decoder_debug PRIVATE
        -O0
        -g3
        -fsanitize=address
        -fsanitize=undefined
)
target_link_options(h264_decoder_debug PRIVATE
        -fsanitize=address
        -fsanitize=undefined
)
target_link_libraries(h264_decoder_debug PRIVATE m)

add_executable(gen_rgb_video src/tests/gen_rgb_video.c)
target_link_libraries(gen_rgb_video PRIVATE m)

add_executable(compare_streams src/tests/streamcomp.c)