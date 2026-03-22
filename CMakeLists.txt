cmake_minimum_required(VERSION 4.0)
project(toy_h264 C)

set(CMAKE_C_STANDARD 11)

add_executable(decoder main.c
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
)

add_executable(annexb_test tests/annexb_test.c
        src/util/bitreader.h
        src/util/bitreader.c
        src/common.h
        src/nal.c
        src/annexb.c
        src/annexb.h
        src/util/expgolomb.c
        src/util/expgolomb.h
        src/ps.h
)


add_executable(golomb_test tests/golomb_test.c
        src/util/bitreader.h
        src/util/bitreader.c
        src/common.h
        src/nal.c
        src/annexb.c
        src/annexb.h
        src/util/expgolomb.c
        src/util/expgolomb.h
        tests/golomb_test.c
)


add_executable(sps_test tests/sps_pps_test.c
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
        tests/sps_pps_test.c
)
add_executable(mb_test tests/mb_test.c)
