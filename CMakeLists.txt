cmake_minimum_required(VERSION 4.0)
project(h264_decoder C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_COMPILER gcc)


# debugging
##
#add_compile_options(-fsanitize=address)
#add_link_options(-fsanitize=address)
##
#add_compile_options(-fsanitize=undefined)
#add_link_options(-fsanitize=undefined)


add_executable(gen_rgb_video src/tests/gen_rgb_video.c)
target_link_libraries(gen_rgb_video m)


add_executable(compare_streams src/tests/streamcomp.c)


add_executable(h264_decoder main.c
        src/intra.c src/intra.h
        src/util/bitreader.h
        src/util/bitreader.c
        src/util/logger.h
        src/util/logger.c
        src/common.h
        src/nal.c
        src/annexb.c
        src/annexb.h
        src/ps.c
        src/util/expgolomb.c
        src/util/expgolomb.h
        src/decoder.c
        src/slice.c
        src/slice.h
        src/util/formulas.h
        src/mb.c
        src/mb.h
        src/util/mbutil.h
        src/util/predutil.h
        src/util/sliceutil.h
        src/cavlc.c
        src/cavlc.h
        src/vlc.c
        src/vlc.h
        src/transform.c
        src/transform.h
        src/global.h
        src/picture.h
        src/picture.c
        src/tests/profiler.c
        src/tests/profiler.h
        src/dequant.c
        src/dequant.h
        src/dpb.c
        src/dpb.h
        src/mv.h
        src/mvpred.h
        src/inter.c
        src/inter.h
        src/qpel.h
        src/qpel.c
        src/deblock.c
        src/deblock.h
)
target_compile_options(h264_decoder PRIVATE -O3 -march=native)
