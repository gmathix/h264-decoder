//
// Created by gmathix on 3/30/26.
//

#include "cavlc.h"
#include "global.h"
#include "vlc.h"
#include "util/expgolomb.h"
#include "util/mbutil.h"

#include <stdio.h>
#include <stdlib.h>

#include "picture.h"


/* Table 9-5 */
const uint16_t coeff_token_lengths[4][62]={
    {
        1,
        6, 2,            8, 6, 3,        9, 8, 7, 5,    10, 9, 8, 6,
        11,10, 9, 7,    13,11,10, 8,    13,13,11, 9,    13,13,13,10,
        14,14,13,11,    14,14,14,13,    15,15,14,14,    15,15,15,14,
        16,15,15,15,    16,16,16,15,    16,16,16,16,    16,16,16,16,
    },
    {
        2,
        6, 2,            6, 5, 3,        7, 6, 6, 4,     8, 6, 6, 4,
        8, 7, 7, 5,      9, 8, 8, 6,    11, 9, 9, 6,    11,11,11, 7,
        12,11,11, 9,    12,12,12,11,    12,12,12,11,    13,13,13,12,
        13,13,13,13,    13,14,13,13,    14,14,14,13,    14,14,14,14,
    },
    {
        4,
        6, 4,            6, 5, 4,        6, 5, 5, 4,     7, 5, 5, 4,
        7, 5, 5, 4,      7, 6, 6, 4,     7, 6, 6, 4,     8, 7, 7, 5,
        8, 8, 7, 6,      9, 8, 8, 7,     9, 9, 8, 8,     9, 9, 9, 8,
        10, 9, 9, 9,    10,10,10,10,    10,10,10,10,    10,10,10,10,
    },
    {
        6,
        6, 6,           6, 6, 6,        6, 6, 6, 6,     6, 6, 6, 6,
        6, 6, 6, 6,     6, 6, 6, 6,     6, 6, 6, 6,     6, 6, 6, 6,
        6, 6, 6, 6,     6, 6, 6, 6,     6, 6, 6, 6,     6, 6, 6, 6,
        6, 6, 6, 6,     6, 6, 6, 6,     6, 6, 6, 6,     6, 6, 6, 6,
    }
};
const uint16_t coeff_token_bits[4][62]={
    {
        1,
        5, 1,           7, 4, 1,        7, 6, 5, 3,     7, 6, 5, 3,
        7, 6, 5, 4,    15, 6, 5, 4,    11,14, 5, 4,     8,10,13, 4,
        15,14, 9, 4,    11,10,13,12,    15,14, 9,12,    11,10,13, 8,
        15, 1, 9,12,    11,14,13, 8,     7,10, 9,12,     4, 6, 5, 8,
    },
    {
        3,
        11, 2,           7, 7, 3,        7,10, 9, 5,     7, 6, 5, 4,
        4, 6, 5, 6,     7, 6, 5, 8,    15, 6, 5, 4,    11,14,13, 4,
        15,10, 9, 4,    11,14,13,12,     8,10, 9, 8,    15,14,13,12,
        11,10, 9,12,     7,11, 6, 8,     9, 8,10, 1,     7, 6, 5, 4,
    },
    {
        15,
        15,14,          11,15,13,        8,12,14,12,    15,10,11,11,
        11, 8, 9,10,     9,14,13, 9,     8,10, 9, 8,    15,14,13,13,
        11,14,10,12,    15,10,13,12,    11,14, 9,12,     8,10,13, 8,
        13, 7, 9,12,     9,12,11,10,     5, 8, 7, 6,     1, 4, 3, 2,
    },
    {
        3,
        0, 1,           4, 5, 6,        8, 9,10,11,    12,13,14,15,
        16,17,18,19,    20,21,22,23,    24,25,26,27,    28,29,30,31,
        32,33,34,35,    36,37,38,39,    40,41,42,43,    44,45,46,47,
        48,49,50,51,    52,53,54,55,    56,57,58,59,    60,61,62,63,
    }
};

static uint16_t total_coeffs[62];
static uint16_t trailing_ones[62];

const uint16_t coeff_token_cr420_lengths[14] = {
     2,
     6,  1,            6,  6,  3,         6,  7,  7,  6,     6,  8,  8,  7
};
const uint16_t coeff_token_cr420_bits[14] = {
    1,
    7,  1,             4,  6,  1,         3,  3,  2,  5,     2,  3,  2,  0
};

const uint16_t coeff_token_cr422_lengths[30] = {
     1,
     7,  2,            7,  7,  3,         9,  7,  7,  5,    9,  9,  7,  6,
    10, 10,  9,  7,   11, 11, 10,  7,    12, 12, 11, 10,   13, 12, 12, 11
};
const uint16_t coeff_token_cr422_bits[30] = {
     1,
    15,  1,            14, 13,  1,        7, 12, 11,  1,    6,  5, 10,  1,
     7,  6,  4,  9,     7,  6,  5,  8,    7,  6,  5,  4,    7,  5,  4,  4
};





/* Table 9-7/8 */
const uint16_t total_zeros_lengths[15][16]= {
    {1,3,3,4,4,5,5,6,6,7,7,8,8,9,9,9},
    {3,3,3,3,3,4,4,4,4,5,5,6,6,6,6},
    {4,3,3,3,4,4,3,3,4,5,5,6,5,6},
    {5,3,4,4,3,3,3,4,3,4,5,5,5},
    {4,4,4,3,3,3,3,3,4,5,4,5},
    {6,5,3,3,3,3,3,3,4,3,6},
    {6,5,3,3,3,2,3,4,3,6},
    {6,4,5,3,2,2,3,3,6},
    {6,6,4,2,2,3,2,5},
    {5,5,3,2,2,2,4},
    {4,4,3,3,1,3},
    {4,4,2,1,3},
    {3,3,1,2},
    {2,2,1},
    {1,1},
};

const uint16_t total_zeros_bits[15][16]= {
    {1,3,2,3,2,3,2,3,2,3,2,3,2,3,2,1},
    {7,6,5,4,3,5,4,3,2,3,2,3,2,1,0},
    {5,7,6,5,4,3,4,3,2,3,2,1,1,0},
    {3,7,5,4,6,5,4,3,3,2,2,1,0},
    {5,4,3,7,6,5,4,3,2,1,1,0},
    {1,1,7,6,5,4,3,2,1,1,0},
    {1,1,5,4,3,3,2,1,1,0},
    {1,1,1,3,3,2,2,1,0},
    {1,0,1,3,2,1,1,1},
    {1,0,1,3,2,1,1},
    {0,1,1,2,1,3},
    {0,1,1,1,1},
    {0,1,1,1},
    {0,1,1},
    {0,1},
};



/* Table 9-9a */
const uint16_t total_zeros_cr420_lengths[3][4] = {
    {1, 2, 3, 3},
    {1, 2, 2},
    {1, 1}
};
const uint16_t total_zeros_cr420_bits[3][4] = {
    {1, 1, 1, 0},
    {1, 1, 0},
    {1, 0}
};


/* Table 9-9b */
const uint16_t total_zeros_cr422_lengths[7][8] = {
    {1, 3, 3, 4, 4, 4, 5, 5},
    {3, 2, 3, 3, 3, 3, 3},
    {3, 3, 2, 2, 3, 3},
    {3, 2, 2, 2, 3},
    {2, 2, 2, 2},
    {2, 2, 1},
    {1, 1}
};
const uint16_t total_zeros_cr422_bits[7][8] = {
    {1, 2, 3, 2, 3, 1, 1, 0},
    {0, 1, 1, 4, 5, 6, 7},
    {0, 1, 1, 2, 6, 7},
    {6, 0, 1, 2, 7},
    {0, 1, 2, 3},
    {0, 1, 1},
    {0, 1}
};


/* Table 9-10 */
const uint16_t run_before_lengths[7][15]={
    {1,1},
    {1,2,2},
    {2,2,2,2},
    {2,2,2,3,3},
    {2,2,3,3,3,3},
    {2,3,3,3,3,3,3},
    {3,3,3,3,3,3,3,4,5,6,7,8,9,10,11},
};

const uint16_t run_before_bits[7][15]={
    {1,0},
    {1,1,0},
    {3,2,1,0},
    {3,2,1,1,0},
    {3,2,3,2,1,0},
    {3,0,1,3,2,5,4},
    {7,6,5,4,3,2,1,1,1,1,1,1,1,1,1},
};






static int vlc_initialized = 0;

static MultiVLC coeff_token_vlc;
static MultiVLC coeff_token_cr420_vlc;
static MultiVLC coeff_token_cr422_vlc;

static MultiVLC total_zeros_vlc;
static MultiVLC total_zeros_cr420_vlc;
static MultiVLC total_zeros_cr422_vlc;

static MultiVLC run_before_vlc;



static const uint8_t nc_to_index[19] = {
    0,0,0,0,1,1,2,2,2,2,3,3,3,3,3,3,3,3,3
};

static const MultiVLC *nc_to_table[19] = {
    /*   -2 */ &coeff_token_cr422_vlc,
    /*   -1 */ &coeff_token_cr420_vlc,
    /* >= 0 */ &coeff_token_vlc, &coeff_token_vlc, &coeff_token_vlc, &coeff_token_vlc,
               &coeff_token_vlc, &coeff_token_vlc, &coeff_token_vlc, &coeff_token_vlc,
               &coeff_token_vlc, &coeff_token_vlc, &coeff_token_vlc, &coeff_token_vlc,
               &coeff_token_vlc, &coeff_token_vlc, &coeff_token_vlc, &coeff_token_vlc,
               &coeff_token_vlc
};



void init_vlc_tables() {
    coeff_token_vlc = make_mutli_vlc(4);
    for (int i = 0; i < 4; i++) {
        set_vlc_table(&coeff_token_vlc, i,
            coeff_token_lengths[i], coeff_token_bits[i],
            62, MAX_COEFF_TOKEN_BITS);
    }


    total_zeros_vlc = make_mutli_vlc(15);
    for (int i = 0; i < 15; i++) {
        set_vlc_table(&total_zeros_vlc, i,
            total_zeros_lengths[i], total_zeros_bits[i],
            16, MAX_TOTAL_ZEROS_BITS);
    }

    total_zeros_cr420_vlc = make_mutli_vlc(3);
    for (int i = 0; i < 3; i++) {
        set_vlc_table(&total_zeros_cr420_vlc, i,
            total_zeros_cr420_lengths[i], total_zeros_cr420_bits[i],
            4, MAX_TOTAL_ZEROS_BITS);
    }

    total_zeros_cr422_vlc = make_mutli_vlc(7);
    for (int i = 0; i < 7; i++) {
        set_vlc_table(&total_zeros_cr422_vlc, i,
            total_zeros_cr422_lengths[i], total_zeros_cr422_bits[i],
            8, MAX_TOTAL_ZEROS_BITS);
    }

    coeff_token_cr420_vlc = make_mutli_vlc(1);
    set_vlc_table(&coeff_token_cr420_vlc, 0,
        coeff_token_cr420_lengths, coeff_token_cr420_bits,
        14, MAX_COEFF_TOKEN_BITS);

    coeff_token_cr422_vlc = make_mutli_vlc(1);
    set_vlc_table(&coeff_token_cr422_vlc, 0,
        coeff_token_cr422_lengths, coeff_token_cr422_bits,
        30, MAX_COEFF_TOKEN_BITS);

    run_before_vlc = make_mutli_vlc(7);
    for (int i = 0; i < 7; i++) {
        set_vlc_table(&run_before_vlc, i,
            run_before_lengths[i], run_before_bits[i],
            15, MAX_RUN_BEFORE_BITS);
    }



    total_coeffs[0]=0; total_coeffs[1]=1; total_coeffs[2]=1;
    total_coeffs[3]=2; total_coeffs[4]=2; total_coeffs[5]=2;
    for (int i = 0; i < 56; i++) total_coeffs[i+6] = 3+i/4;

    trailing_ones[0]=0; trailing_ones[1]=0; trailing_ones[2]=1;
    trailing_ones[3]=0; trailing_ones[4]=1; trailing_ones[5]=2;
    for (int i = 0; i < 56; i++) trailing_ones[i+6] = i%4;


    vlc_initialized = 1;
}

/* 7.3.5.3.2 */
void residual_block_cavlc(Macroblock *mb, int blkIdx, int iCbCr, int bt, int16_t coeffLevel[], uint8_t (*total_coeffs_table)[16],
    int startIdx, int endIdx, int maxNumCoeff, bool isLuma, SliceHeader *sh, CodecContext *ctx) {

    BitReader *br = ctx->br;


    if (sh->pps->entropy_coding_mode_flag) {
        printf("CABAC enabled and trying to call CAVLC ? bold move\n");
    }

    if (!vlc_initialized) {
        init_vlc_tables();
    }

    for (int i = 0; i < maxNumCoeff; i++) {
        coeffLevel[i] = 0;
    }



    int totalCoeff;
    int trailingOnes;
    int nC;
    coeff_token(mb, blkIdx, iCbCr, bt, &startIdx, &endIdx, isLuma, &totalCoeff, &trailingOnes, &nC, total_coeffs_table, sh, ctx);


    if (totalCoeff > 0) {
        int16_t levelVal[totalCoeff];
        parse_level(levelVal, blkIdx, bt, totalCoeff, trailingOnes, ctx);

        int16_t runVal[totalCoeff];
        parse_run(runVal, blkIdx, bt, totalCoeff, maxNumCoeff, startIdx, endIdx, sh, ctx);

        reconstruct(levelVal, blkIdx, bt, runVal, coeffLevel, startIdx, totalCoeff);
    }
}


void coeff_token(Macroblock *mb, int blkIdx, int iCbCr, int bt, int *startIdx, int *endIdx, bool isLuma,
                    int *totalCoeff, int *trailingOnes, int *nC, uint8_t (*total_coeffs_table)[16], SliceHeader *sh, CodecContext *ctx) {

    BitReader *br = ctx->br;

    if (bt == CHROMA_DC_LEVEL) *nC = sh->sps->chroma_format_idc * -1;

    int nA = 0, nB = 0;
    Neighbors n = isLuma
        ? derive_neighbors_4x4_luma(mb, blkIdx, ctx)
        : derive_neighbors_4x4_chroma(mb, blkIdx, ctx);


    if (n.a_av) {
        nA = total_coeffs_table[mb->mbAddr + n.mb_a_off][n.a_idx];
    }
    if (n.b_av) {
        nB = total_coeffs_table[mb->mbAddr + n.mb_b_off][n.b_idx];
    }


    if (bt != CHROMA_DC_LEVEL) {
        if (n.a_av && n.b_av)  *nC = (nA+nB+1)>>1;
        else *nC = n.a_av*nA + n.b_av * nB;
    }

    unsigned bits = bitreader_peek_bits(br, MAX_COEFF_TOKEN_BITS);
    int length = get_vlc_length(nc_to_table[*nC + 2], nc_to_index[*nC + 2], br);

    int sym = get_vlc(nc_to_table[*nC + 2], nc_to_index[*nC + 2], br);

    *totalCoeff = total_coeffs[sym];
    *trailingOnes = trailing_ones[sym];


    /* luma 16x16 DC doesn't store totalCoeff */
    if (bt != LUMA_INTRA_16x16_DC_LEVEL) {
        total_coeffs_table[mb->mbAddr][blkIdx] = *totalCoeff;
    }
}


/* 7.3.5.3.2 */
void parse_level(int16_t levelVal[], int blkIdx, int bt, int totalCoeff, int trailingOnes, CodecContext *ctx) {
    BitReader *br = ctx->br;



    int suffixLength = totalCoeff > 10 && trailingOnes < 3 ? 1 : 0;

    for (int i = 0; i < totalCoeff; i++) {
        if (i < trailingOnes) {

            unsigned sign_bit = read_u(br, 1);
            levelVal[i] = 1 - 2*sign_bit;

        } else {
            /* 1 */
            int leadingZeroBits = -1;
            for (uint32_t b = 0; !b; leadingZeroBits++)
                b = read_u(br, 1);
            uint16_t level_prefix = leadingZeroBits;

            /* 2 */
            uint16_t suffix_size;
            if (level_prefix == 14 && suffixLength == 0)  suffix_size = 4;
            else if (level_prefix >= 15)                  suffix_size = level_prefix - 3;
            else                                          suffix_size = suffixLength;

            /* 3 */
            uint16_t level_suffix;
            if (suffix_size > 0)  level_suffix = read_u(br, suffix_size);
            else                  level_suffix = 0;

            /* 4 */
            uint16_t levelCode = (_min(15, level_prefix) << suffixLength) + level_suffix;

            /* 5 */
            if (level_prefix >= 15 && suffixLength == 0) levelCode += 15;
            /* 6 */
            if (level_prefix >= 16) levelCode += (1 << (level_prefix - 3)) - 4096;
            /* 7 */
            if (i == trailingOnes && trailingOnes < 3) levelCode += 2;

            /* 8 */
            if (levelCode % 2 == 0) levelVal[i] = (levelCode+2) >> 1;
            else    levelVal[i] = (-(int16_t)levelCode-1) >> 1;


            int suffixLength_used = suffixLength;
            /* 9 */
            if (suffixLength == 0) suffixLength = 1;

            /* 10 */
            if (_abs(levelVal[i]) > (3 << (suffixLength - 1)) && suffixLength < 6) suffixLength += 1;
        }
    }

}


void parse_run(int16_t runVal[],int blkIdx,  int bt, int totalCoeff, int maxNumCoeff, int startIdx, int endIdx, SliceHeader *sh, CodecContext *ctx) {
    BitReader *br = ctx->br;


    int16_t zerosLeft;

    if (totalCoeff < endIdx - startIdx + 1) {
        MultiVLC table;
        if (maxNumCoeff <= 8) {
            if (maxNumCoeff == 4) {
                table = total_zeros_cr420_vlc;
            } else {
                table = total_zeros_cr422_vlc;
            }
        } else {
            table = total_zeros_vlc;
        }
        zerosLeft = (int16_t)get_vlc(&table, totalCoeff-1, br);
    } else {
        zerosLeft = 0;
    }

    for (int i = 0; i < totalCoeff-1; i++) {
        if (zerosLeft > 0) {
            int idx = zerosLeft > 6 ? 6 : zerosLeft-1;
            uint16_t run_before = get_vlc(&run_before_vlc, idx, br);
            runVal[i] = (int16_t)run_before;
        } else {
            runVal[i] = 0;
        }
        zerosLeft -= runVal[i];
    }
    runVal[totalCoeff-1] = zerosLeft;
}


void reconstruct(const int16_t levelVal[], int blkIdx, int bt,  const int16_t runVal[], int16_t coeffLevel[], int startIdx, int totalCoeff) {
    int coeffNum = -1;
    for (int i = totalCoeff-1; i >= 0; i--) {
        coeffNum += runVal[i] + 1;
        coeffLevel[startIdx + coeffNum] = levelVal[i];
    }
}