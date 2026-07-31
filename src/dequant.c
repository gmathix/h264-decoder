//
// Created by gmathix on 4/15/26.
//

#include "dequant.h"

#include "util/bitreader.h"
#include "util/expgolomb.h"


/* default scaling lists */

const int flat_4x4_16[16] = {
    16, 16, 16, 16,
    16, 16, 16, 16,
    16, 16, 16, 16,
    16, 16, 16, 16
};

const int default_4x4_intra[16] = {
     6, 13, 13, 20,
    20, 20, 28, 28,
    28, 28, 32, 32,
    32, 37, 37, 42
};

const int default_4x4_inter[16] = {
    10, 14, 14, 20,
    20, 20, 24, 24,
    24, 24, 27, 27,
    27, 30, 30, 34,
};

const int flat_8x8_16[64] = {
    16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16,
};

const int default_8x8_intra[64] = {
     6, 10, 10, 13, 11, 13, 16, 16,
    16, 16, 18, 18, 18, 18, 18, 23,
    23, 23, 23, 23, 23, 25, 25, 25,
    25, 25, 25, 25, 27, 27, 27, 27,
    27, 27, 27, 27, 29, 29, 29, 29,
    29, 29, 29, 31, 31, 31, 31, 31,
    31, 33, 33, 33, 33, 33, 36, 36,
    36, 36, 38, 38, 38, 40, 40, 42
};

const int default_8x8_inter[64] = {
     9, 13, 13, 15, 13, 15, 17, 17,
    17, 17, 19, 19, 19, 19, 19, 21,
    21, 21, 21, 21, 21, 22, 22, 22,
    22, 22, 22, 22, 24, 24, 24, 24,
    24, 24, 24, 24, 25, 25, 25, 25,
    25, 25, 25, 27, 27, 27, 27, 27,
    27, 28, 28, 28, 28, 28, 30, 30,
    30, 30, 32, 32, 32, 33, 33, 35
};



const int norm_adjust_4x4[6][3] = {
    {10, 13, 16},
    {11, 14, 18},
    {13, 16, 20},
    {14, 18, 23},
    {16, 20, 25},
    {18, 23, 29},
};

const int norm_adjust_8x8[6][6] = {
    {20, 18, 32, 19, 25, 24},
    {22, 19, 35, 21, 28, 26},
    {26, 23, 42, 24, 33, 31},
    {28, 25, 45, 26, 35, 33},
    {32, 28, 51, 30, 40, 38},
    {36, 32, 58, 34, 46, 43},
};


const uint8_t scaling_list_indices[2][2][3] = { // [intra/inter][4x4/8x8][Y/Cb/Cr]
    { {0, 1, 2}, { 6, 8, 10} },
    { {3, 4, 5}, { 7, 9, 11} },
};


const int matrix_scan_4x4[16] = {
    0,  1,  5,  6,
    2,  4,  7, 12,
    3,  8, 11, 13,
    9, 10, 14, 15
};

const int matrix_scan_8x8[64] = {
     0,  1,  5,  6, 14, 15, 27, 28,
     2,  4,  7, 13, 16, 26, 29, 42,
     3,  8, 12, 17, 25, 30, 41, 43,
     9, 11, 18, 24, 31, 40, 44, 53,
    10, 19, 23, 32, 39, 45, 52, 54,
    20, 22, 33, 38, 46, 51, 55, 60,
    21, 34, 37, 47, 50, 56, 59, 61,
    35, 36, 48, 49, 57, 58, 62, 63
};



void copy_scaling_list(int *list, int size, int index, bool is4x4, Undo264Context *ctx) {
    for (int i = 0; i < size; i++) {
        if (is4x4) {
            ctx->scalingList4x4[index][i] = list[i];
        } else {
            ctx->scalingList8x8[index-6][i] = list[i];
        }
    }
}

void parse_scaling_list(int *scaling_list, int size, bool *useDefault, BitReader *br) {
    int lastScale = 8;
    int nextScale = 8;
    for (int i = 0; i < size; i++) {
        if (nextScale != 0) {
            int delta_scale = read_se(br);
            nextScale = (lastScale + delta_scale + 256) % 256;
            *useDefault = i == 0 && nextScale == 0;
        }
        scaling_list[i] = nextScale == 0 ? lastScale : nextScale;
        lastScale = scaling_list[i];
    }
}

void scaling_list_fallback(int index, bool is4x4, bool isPPS, Undo264Context *ctx) {
    bool useDefault = is4x4 ? ctx->useDefaultList4x4[index] : ctx->useDefaultList8x8[index-6];
    bool fallbackB = isPPS && ctx->seqScalingListPresent;

    if (index == 0) {
        if (useDefault)      copy_scaling_list(default_4x4_intra, 16, 0, true, ctx);
        else if (!fallbackB) copy_scaling_list(default_4x4_intra, 16, 0, true, ctx);
        else                 copy_scaling_list(ctx->scalingList4x4[0], 16, 0, true, ctx);
    } else if (index == 1) {
        if (useDefault)      copy_scaling_list(default_4x4_intra, 16, 1, true, ctx);
        else if (!fallbackB) copy_scaling_list(ctx->scalingList4x4[0], 16, 1, true, ctx);
        else                 copy_scaling_list(ctx->scalingList4x4[0], 16, 1, true, ctx);
    } else if (index == 2) {
        if (useDefault)      copy_scaling_list(default_4x4_intra, 16, 2, true, ctx);
        else if (!fallbackB) copy_scaling_list(ctx->scalingList4x4[1], 16, 2, true, ctx);
        else                 copy_scaling_list(ctx->scalingList4x4[1], 16, 2, true, ctx);
    } else if (index == 3) {
        if (useDefault)      copy_scaling_list(default_4x4_inter, 16, 3, true, ctx);
        else if (!fallbackB) copy_scaling_list(default_4x4_inter, 16, 3, true, ctx);
        else                 copy_scaling_list(ctx->scalingList4x4[3], 16, 3, true, ctx);
    } else if (index == 4) {
        if (useDefault)      copy_scaling_list(default_4x4_inter, 16, 4, true, ctx);
        else if (!fallbackB) copy_scaling_list(ctx->scalingList4x4[3], 16, 4, true, ctx);
        else                 copy_scaling_list(ctx->scalingList4x4[3], 16, 4, true, ctx);
    } else if (index == 5) {
        if (useDefault)      copy_scaling_list(default_4x4_inter, 16, 4, true, ctx);
        else if (!fallbackB) copy_scaling_list(ctx->scalingList4x4[4], 16, 5, true, ctx);
        else                 copy_scaling_list(ctx->scalingList4x4[4], 16, 5, true, ctx);
    } else if (index == 6) {
        if (useDefault)      copy_scaling_list(default_8x8_intra, 64, 6, false, ctx);
        else if (!fallbackB) copy_scaling_list(default_8x8_intra, 64, 6, false, ctx);
        else                 copy_scaling_list(ctx->scalingList8x8[0], 64, 6, false, ctx);
    } else if (index == 7) {
        if (useDefault)      copy_scaling_list(default_8x8_inter, 64, 7, false, ctx);
        else if (!fallbackB) copy_scaling_list(default_8x8_inter, 64, 7, false, ctx);
        else                 copy_scaling_list(ctx->scalingList8x8[1], 64, 7, false, ctx);
    }
}

void infer_flat_matrices(Undo264Context *ctx) {
    for (int i = 0; i < 6; i++) {
        copy_scaling_list(flat_4x4_16, 16, i, true, ctx);
    }
    for (int i = 6; i < 8; i++) {
        copy_scaling_list(flat_8x8_16, 64, i, false, ctx);
    }
}



static ALWAYS_INLINE void inverse_matrix_scan(
    int size, int scan[static size], int in[static size], int out[static size]) {

   for (int i = 0; i < size; i++) {
       out[i] = in[scan[i]];
   }
}


void precompute_4x4_scales(Undo264Context *ctx) {
    int matrix[16];

    for (int n = 0; n < 6; n++) {

        inverse_matrix_scan(16, matrix_scan_4x4, ctx->scalingList4x4[n], matrix);

        for (int qp = 0; qp <= 51; qp++) {
            for (int i = 0; i < 4; i++) {
                for (int j = 0; j < 4; j++) {
                    ctx->levelScale4x4[n][qp][i][j] = norm_adjust_4x4[qp%6][(i&1) + (j&1)] * matrix[(i<<2) + j];
                }
            }
        }
    }
}


void precompute_8x8_scales(Undo264Context *ctx) {
    int matrix[64];

    for (int n = 0; n < 2; n++) {

        inverse_matrix_scan(64, matrix_scan_8x8, ctx->scalingList8x8[n], matrix);

        for (int qp = 0; qp <= 51; qp++) {
            for (int i = 0; i < 8; i++) {
                for (int j = 0; j < 8; j++) {
                    int idx;
                    if      (i%4 == 0 && j%4 == 0) idx = 0;
                    else if (i%2 == 1 && j%2 == 1) idx = 1;
                    else if (i%4 == 2 && j%4 == 2) idx = 2;
                    else if (i%4 == 0 && j%2 == 1) idx = 3;
                    else if (i%2 == 1 && j%4 == 0) idx = 3;
                    else if (i%4 == 0 && j%4 == 2) idx = 4;
                    else if (i%4 == 2 && j%4 == 0) idx = 4;
                    else                           idx = 5;
                    ctx->levelScale8x8[n][qp][i][j] = norm_adjust_8x8[qp%6][idx] * matrix[(i<<3) + j];
                }
            }
        }
    }
}