//
// Created by gmathix on 3/30/26.
//

#ifndef TOY_H264_CAVLC_H
#define TOY_H264_CAVLC_H

#include "mb.h"
#include "util/bitreader.h"
#include "slice.h"

#include <stdint.h>


#define MAX_CODE_LENGTH   16


typedef enum {


    LUMA_INTRA_16x16_DC_LEVEL,
    LUMA_INTRA_16x16_AC_LEVEL,

    CB_INTRA_16x16_DC_LEVEL,
    CB_INTRA_16x16_AC_LEVEL,

    CR_INTRA_16x16_DC_LEVEL,
    CR_INTRA_16x16_AC_LEVEL,

    LUMA_LEVEL_4x4,

    CHROMA_DC_LEVEL,
    CHROMA_AC_LEVEL,

    CB_LEVEL_4x4,
    CR_LEVEL_4x4,

    LUMA_LEVEL_8x8,

} BlockType ;


#define MAX_COEFF_TOKEN_BITS    16
#define MAX_TOTAL_ZEROS_BITS     9
#define MAX_RUN_BEFORE_BITS     11


extern const uint16_t coeff_token_lengths      [4][62];
extern const uint16_t coeff_token_bits         [4][62];

extern const uint16_t coeff_token_cr420_lengths   [14];
extern const uint16_t coeff_token_cr420_bits      [14];

extern const uint16_t coeff_token_cr422_lengths   [30];
extern const uint16_t coeff_token_cr422_bits      [30];

extern const uint16_t total_zeros_lengths     [15][16];
extern const uint16_t total_zeros_bits        [15][16];

extern const uint16_t total_zeros_cr420_lengths [3][4];
extern const uint16_t total_zeros_cr420_bits    [3][4];
extern const uint16_t total_zeros_cr422_lengths [7][8];
extern const uint16_t total_zeros_cr422_bits    [7][8];

extern const uint16_t run_before_lengths        [7][15];
extern const uint16_t run_before_bits           [7][15];


static char* bt_to_string(int bt) {
    switch (bt) {
        case LUMA_INTRA_16x16_DC_LEVEL: return "Lum16DC";
        case LUMA_INTRA_16x16_AC_LEVEL: return "Lum16AC";
        case CB_INTRA_16x16_DC_LEVEL: return "Cb16DC";
        case CB_INTRA_16x16_AC_LEVEL: return "Cb16AC";
        case CR_INTRA_16x16_DC_LEVEL: return "Cr16DC";
        case CR_INTRA_16x16_AC_LEVEL: return "Cr16AC";
        case LUMA_LEVEL_4x4: return "Lum4";
        case CHROMA_DC_LEVEL: return "ChrDC";
        case CHROMA_AC_LEVEL: return "ChrAC";
        case CB_LEVEL_4x4: return "Cb4";
        case CR_LEVEL_4x4: return "Cr4";
        case LUMA_LEVEL_8x8: return "Luma8";
        default: return "Block";
    }
}


void  residual_block_cavlc (Macroblock *mb, int blkIdx, int iCbCr, int bt, int16_t coeffLevel[], uint8_t (*total_coeffs_table)[16], int startIdx, int endIdx, int maxNumCoeff, bool isLuma, SliceHeader *sh, CodecContext *ctx);


void  coeff_token    (Macroblock *mb, int blkIdx, int iCbCr, int bt, int *startIdx, int *endIdx, bool isLuma,
                        int *totalCoeff, int *trailingOnes, int *nC, uint8_t (*total_coeffs_table)[16], SliceHeader *sh, CodecContext *ctx);
void  parse_level    (int16_t levelVal[], int blkIdx, int bt, int totalCoeff, int trailingOnes, CodecContext *ctx);
void  parse_run      (int16_t runVal[], int blkIdx, int bt,  int totalCoeff, int maxNumCoeff, int startIdx, int endIdx, SliceHeader *sh, CodecContext *ctx);
void  reconstruct    (const int16_t levelVal[], int blkIdx, int bt, const int16_t runVal[], int16_t coeffLevel[], int startIdx, int totalCoeff);

#endif //TOY_H264_CAVLC_H