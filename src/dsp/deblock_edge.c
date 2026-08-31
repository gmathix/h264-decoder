//
// Created by gmathix on 8/31/26.
//


#include "deblock.h"
#include "global.h"


always_inline flatten void deblock_edge_low_bs_luma(uint8_t *dst, int xstride, int ystride, int alpha, int beta, int indexA, int *bS) {

    uint8_t p0, p1, p2, q0, q1, q2;
    int aP, aQ;

    for (int edge = 0; edge < 4; edge++) {
        if (bS[edge] == 0) {
            dst += 4*xstride;
            continue;
        }
        int tc0 = treshold_table[bS[edge]-1][indexA];

        for (int i = 0; i < 4; i++) {
            p0 = dst[-1*ystride];  p1 = dst[-2*ystride];  p2 = dst[-3*ystride];
            q0 = dst[ 0*ystride];  q1 = dst[ 1*ystride];  q2 = dst[ 2*ystride];

            if (ABS(p0 - q0) < alpha &&
                ABS(p1 - p0) < beta &&
                ABS(q1 - q0) < beta) {

                aP = ABS(p2 - p0);
                aQ = ABS(q2 - q0);

                int t = tc0 + ((aP < beta) + (aQ < beta));
                int delta = CLIP3(-t, t, (((q0 - p0) * (1 << 2)) + (p1 - q1) + 4) >> 3);

                if (aP < beta) {
                    /*p1*/ dst[-2*ystride] += CLIP3(-tc0, tc0, (p2 + ((p0 + q0 + 1) >> 1) - (p1 << 1)) >> 1);
                }
                if (aQ < beta) {
                    /*q1*/ dst[ 1*ystride] += CLIP3(-tc0, tc0, (q2 + ((p0 + q0 + 1) >> 1) - (q1 << 1)) >> 1);
                }
                /*p0*/     dst[-1*ystride] = CLIP3(0, MAX_U8, p0 + delta);
                /*q0*/     dst[ 0*ystride] = CLIP3(0, MAX_U8, q0 - delta);
            }

            dst += xstride;
        }
    }
}

always_inline flatten void deblock_edge_high_bs_luma(uint8_t *dst, int xstride, int ystride, int alpha, int beta) {

    uint8_t p0, p1, p2, p3, q0, q1, q2, q3;
    int aP, aQ;

    for (int edge = 0; edge < 4; edge++) {
        for (int i = 0; i < 4; i++) {
            p0 = dst[-1*ystride];  p1 = dst[-2*ystride];  p2 = dst[-3*ystride];  p3 = dst[-4*ystride];
            q0 = dst[ 0*ystride];  q1 = dst[ 1*ystride];  q2 = dst[ 2*ystride];  q3 = dst[ 3*ystride];

            if (ABS(p0 - q0) < alpha &&
                ABS(p1 - p0) < beta &&
                ABS(q1 - q0) < beta) {

                aP = ABS(p2 - p0);
                aQ = ABS(q2 - q0);

                if (aP < beta && ABS(p0 - q0) < ((alpha >> 2) + 2)) {
                    /*p0*/ dst[-1*ystride] = (p2 + 2*p1 + 2*p0 + 2*q0 + q1 + 4) >> 3;
                    /*p1*/ dst[-2*ystride] = (p2 + p1 + p0 + q0 + 2) >> 2;
                    /*p2*/ dst[-3*ystride] = (2*p3 + 3*p2 + p1 + p0 + q0 + 4) >> 3;
                } else {
                    /*p0*/ dst[-1*ystride] = (2*p1 + p0 + q1 + 2) >> 2;
                }

                if (aQ < beta && ABS(p0 - q0) < ((alpha >> 2) + 2)) {
                    /*q0*/ dst[ 0*ystride] = (p1 + 2*p0 + 2*q0 + 2*q1 + q2 + 4) >> 3;
                    /*q1*/ dst[ 1*ystride] = (p0 + q0 + q1 + q2 + 2) >> 2;
                    /*q2*/ dst[ 2*ystride] = (2*q3 + 3*q2 + q1 + q0 + p0 + 4) >> 3;
                } else {
                    /*q0*/ dst[ 0*ystride] = (2*q1 + q0 + p1 + 2) >> 2;
                }
            }

            dst += xstride;
        }
    }
}

always_inline flatten void deblock_edge_low_bs_chroma(uint8_t *dst, int xstride, int ystride, int alpha, int beta, int indexA, int *bS) {

    uint8_t p0, p1, q0, q1;

    for (int edge = 0; edge < 4; edge++) {
        if (bS[edge] == 0) {
            dst += 2*xstride;
            continue;
        }
        int tc0 = treshold_table[bS[edge]-1][indexA];

        for (int i = 0; i < 2; i++) {
            p0 = dst[-1*ystride];  p1 = dst[-2*ystride];
            q0 = dst[ 0*ystride];  q1 = dst[ 1*ystride];

            if (ABS(p0 - q0) < alpha &&
                ABS(p1 - p0) < beta &&
                ABS(q1 - q0) < beta) {

                int t = tc0 + 1;
                int delta = CLIP3(-t, t, (((q0 - p0) * (1 << 2)) + (p1 - q1) + 4) >> 3);

                /*p0*/     dst[-1*ystride] = CLIP3(0, MAX_U8, p0 + delta);
                /*q0*/     dst[ 0*ystride] = CLIP3(0, MAX_U8, q0 - delta);
            }

            dst += xstride;
        }
    }
}

always_inline flatten void deblock_edge_high_bs_chroma(uint8_t *dst, int xstride, int ystride, int alpha, int beta) {

    uint8_t p0, p1, q0, q1;

    for (int edge = 0; edge < 4; edge++) {
        for (int i = 0; i < 2; i++) {
            p0 = dst[-1*ystride];  p1 = dst[-2*ystride];
            q0 = dst[ 0*ystride];  q1 = dst[ 1*ystride];

            if (ABS(p0 - q0) < alpha &&
                ABS(p1 - p0) < beta &&
                ABS(q1 - q0) < beta) {

                /*p0*/ dst[-1*ystride] = (2*p1 + p0 + q1 + 2) >> 2;
                /*q0*/ dst[ 0*ystride] = (2*q1 + q0 + p1 + 2) >> 2;
            }

            dst += xstride;
        }
    }
}
