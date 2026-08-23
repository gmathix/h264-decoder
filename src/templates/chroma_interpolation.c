//
// Created by gmathix on 8/23/26.
//

#include "stdint.h"

#ifndef WIDTH
#define WIDTH 8
#endif
#ifndef HEIGHT
#define HEIGHT 8
#endif

#define INTER_CHROMA_FUNC3(name, W, H, ...) name ## _ ## W ## x ## H(__VA_ARGS__)
#define INTER_CHROMA_FUNC2(name, W, H, ...) INTER_CHROMA_FUNC3(name, W, H, __VA_ARGS__)
#define INTER_CHROMA_FUNC(name, ...) INTER_CHROMA_FUNC2(name, WIDTH, HEIGHT, __VA_ARGS__)


void INTER_CHROMA_FUNC(chroma_interpolation,
                       const uint8_t * restrict temp_buf, uint8_t * restrict dst, int stride,
                       int xFrac, int yFrac) {
    for (int i = 0; i < HEIGHT; i++) {
        for (int j = 0; j < WIDTH; j++) {
            dst[j] =   ((8-xFrac) * (8-yFrac) * temp_buf[(i+2)*(WIDTH+5) + 2+j]     +
                           xFrac  * (8-yFrac) * temp_buf[(i+2)*(WIDTH+5) + 2+j+1]   +
                        (8-xFrac) *    yFrac  * temp_buf[(i+3)*(WIDTH+5) + 2+j]   +
                           xFrac  *    yFrac  * temp_buf[(i+3)*(WIDTH+5) + 2+j+1] + 32) >> 6;
        }
        dst += stride;
    }
}