cmake_minimum_required(VERSION 4.0)
project(toy_h264 C)

set(CMAKE_C_STANDARD 11)

add_executable(decoder main.c
        src/bitreader.h
        src/bitreader.c
        src/common.h
        src/nal.c
        src/annexb.c
        src/annexb.h
)

add_executable(annexb_test tests/annexb_test.c
        src/bitreader.h
        src/bitreader.c
        src/common.h
        src/nal.c
        src/annexb.c
        src/annexb.h
        src/expgolomb.c
        src/expgolomb.h
        tests/golomb_test.c
)


add_executable(golomb_test tests/golomb_test.c
        src/bitreader.h
        src/bitreader.c
        src/common.h
        src/nal.c
        src/annexb.c
        src/annexb.h
        src/expgolomb.c
        src/expgolomb.h
        tests/golomb_test.c
)
