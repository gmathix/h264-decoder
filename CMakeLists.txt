cmake_minimum_required(VERSION 4.0)
project(toy_h264 C)

set(CMAKE_C_STANDARD 11)

add_compile_options(-fsanitize=address)
add_link_options(-fsanitize=address)



add_executable(cavlc_decode main.c
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
)

add_executable(annexb_test src/tests/annexb_test.c
        src/util/bitreader.h
        src/util/bitreader.c
        src/common.h
        src/nal.c
        src/annexb.c
        src/annexb.h
        src/util/expgolomb.c
        src/util/expgolomb.h
        src/ps.h
        src/util/logger.c
        src/util/logger.h
)


add_executable(golomb_test src/tests/golomb_test.c
        src/util/bitreader.h
        src/util/bitreader.c
        src/common.h
        src/nal.c
        src/annexb.c
        src/annexb.h
        src/util/expgolomb.c
        src/util/expgolomb.h
        src/tests/golomb_test.c
)


add_executable(sps_test src/tests/sps_pps_test.c
        src/util/bitreader.h
        src/util/bitreader.c
        src/nal.c
        src/annexb.c
        src/annexb.h
        src/util/expgolomb.c
        src/util/expgolomb.h
        src/ps.h
        src/ps.c
        src/common.h
        src/tests/sps_pps_test.c
)
add_executable(mb_test src/tests/mb_test.c)

add_executable(cavlc_test src/tests/cavlc_test.c
        src/util/bitreader.h
        src/util/bitreader.c
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

)
add_executable(gen_rgb_video src/tests/gen_rgb_video.c)
target_link_libraries(gen_rgb_video m)