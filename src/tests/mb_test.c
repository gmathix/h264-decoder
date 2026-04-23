//
// Created by gmathix on 3/20/26.
//

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "time.h"





int main(void) {
    int SIZE = 500;
    int top_samples[SIZE];
    int left_samples[SIZE];
    int pred[SIZE][SIZE];

    struct timespec ts, te;

    // warmup
    for (int k = 0; k < 100; k++) {
        for (int i = 0; i < SIZE; i++) {
            top_samples[i] = i;
            left_samples[i] = SIZE-i;
        }
    }


    // method 1 : condition in loop body
    clock_gettime(CLOCK_MONOTONIC, &ts);
    for (int i = 0; i < 1000; i++) {
        for (int y = 0; y < SIZE; y++) {
            for (int x = 0; x < SIZE; x++) {
                if (y==x) {
                    pred[y][x] = top_samples[x] + left_samples[y];
                }
                else if (x>y) {
                    pred[y][x] = top_samples[y];
                }
                else {
                    pred[y][x] = left_samples[y];
                }
            }
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &te);
    printf("method 1 took %.3fms\n",
        (double)(te.tv_sec-ts.tv_sec)*1000 + (double)(te.tv_nsec-ts.tv_nsec)/1000000);


    // method 2 : fill main diagonal first, then upper triangle, then lower triangle
    clock_gettime(CLOCK_MONOTONIC, &ts);
    for (int i = 0; i < 1000; i++) {
        for (int d = 0; d < SIZE; d++) {
            pred[d][d] = top_samples[d] + left_samples[d];
        }
        for (int y = 0; y < SIZE; y++) {
            for (int x = y+1; x < SIZE; x++) {
                pred[y][x] = top_samples[y];
            }
        }
        for (int y = 1; y < SIZE; y++) {
            for (int x = 0; x < y; x++) {
                pred[y][x] = left_samples[y];
            }
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &te);
    printf("method 2 took %.3fms\n",
        (double)(te.tv_sec-ts.tv_sec)*1000 + (double)(te.tv_nsec-ts.tv_nsec)/1000000);




    return 0;
}