//
// Created by gmathix on 3/20/26.
//

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "time.h"


const int32_t WIDTH = 1920;
const int32_t HEIGHT = 1080;


int main(void) {
    uint8_t *frame = malloc(WIDTH * HEIGHT * sizeof(uint8_t));

    for (int i = 0; i < WIDTH * HEIGHT; i++)
        frame[i] = i % UINT8_MAX;


    int mb_width = WIDTH / 16;
    int mb_height = HEIGHT / 16;

    uint8_t (*mbs)[mb_height * mb_width][16][16] = malloc(mb_width * mb_height * sizeof(*mbs));


    struct timespec ts, te;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    for (int i = 0; i < mb_width * mb_height; i++) {
        int mb_y = i / mb_width;
        int mb_x = i % mb_width;

        for (int j = 0; j < 16; j++) {
            int off = mb_y * WIDTH + mb_x * 16 + j * WIDTH;
            memcpy(mbs[i][j], frame + off, 16 * sizeof(uint8_t));
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &te);

    double total = (te.tv_sec - ts.tv_sec) * 1000 +
        (te.tv_nsec-ts.tv_nsec) / 1e6;

    printf("macroblock division took %.3fms\n", total);

    printf("%d\n", (int32_t) 3.9999);

    free(frame);

    return 0;
}