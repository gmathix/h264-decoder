//
// Created by gmathix on 8/23/26.
//


#include "global.h"
#include "util/formulas.h"


#ifndef WIDTH
#define WIDTH 16
#endif
#ifndef HEIGHT
#define HEIGHT 16
#endif


#define INTER_FUNC3(name, W, H, ...) name ## _ ## W ## x ## H(__VA_ARGS__)
#define INTER_FUNC2(name, W, H, ...) INTER_FUNC3(name, W, H, __VA_ARGS__)
#define INTER_FUNC(name, ...) INTER_FUNC2(name, WIDTH, HEIGHT, __VA_ARGS__)



void INTER_FUNC(weigh_single,
                uint8_t *dst, int stride, int logWD, int w, int o) {
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            dst[x] = _clip1y(((dst[x] * w + (logWD > 0) * (1 << (logWD-1))) >> logWD) + o, MAX_U8);
        }
        dst += stride;
    }
}

void INTER_FUNC(weigh_bi,
                const uint8_t *restrict temp_bi_buf, uint8_t *restrict dst, int stride,
                int logWD, int w0, int w1, int o0, int o1) {
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            int t0 = temp_bi_buf[y*WIDTH + x];
            int t1 = temp_bi_buf[WIDTH*HEIGHT + y*WIDTH + x];
            dst[x] = (uint8_t) _clip1y(((t0 * w0 + t1 * w1 + (1<<logWD)) >>
                                          (logWD + 1)) + ((o0 + o1 + 1) >> 1), MAX_U8);
        }
        dst += stride;
    }
}