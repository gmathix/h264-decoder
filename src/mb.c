//
// Created by gmathix on 3/21/26.
//

#include "cavlc.h"
#include "mb.h"

#include <stdio.h>

#include "util/expgolomb.h"
#include "util/formulas.h"
#include "util/mbutil.h"
#include "util/predutil.h"

#include <stdlib.h>
#include <threads.h>
#include <unistd.h>




/* table 7-11 */
const I_MbInfo i_mb_type_info[26] = {
    /*  0 */ {MB_TYPE_INTRA4x4,   -1,        -1, -1, },
    /*  1 */ {MB_TYPE_INTRA16x16, VERT_PRED,  0,  0, },
    /*  2 */ {MB_TYPE_INTRA16x16, HOR_PRED,   0,  0, },
    /*  3 */ {MB_TYPE_INTRA16x16, DC_PRED,    0,  0, },
    /*  4 */ {MB_TYPE_INTRA16x16, PLANE_PRED, 0,  0, },
    /*  5 */ {MB_TYPE_INTRA16x16, VERT_PRED,  1,  0, },
    /*  6 */ {MB_TYPE_INTRA16x16, HOR_PRED,   1,  0, },
    /*  7 */ {MB_TYPE_INTRA16x16, DC_PRED,    1,  0, },
    /*  8 */ {MB_TYPE_INTRA16x16, PLANE_PRED, 1,  0, },
    /*  9 */ {MB_TYPE_INTRA16x16, VERT_PRED,  2,  0, },
    /* 10 */ {MB_TYPE_INTRA16x16, HOR_PRED,   2,  0, },
    /* 11 */ {MB_TYPE_INTRA16x16, DC_PRED,    2,  0, },
    /* 12 */ {MB_TYPE_INTRA16x16, PLANE_PRED, 2,  0, },
    /* 13 */ {MB_TYPE_INTRA16x16, VERT_PRED,  0, 15, },
    /* 14 */ {MB_TYPE_INTRA16x16, HOR_PRED,   0, 15, },
    /* 15 */ {MB_TYPE_INTRA16x16, DC_PRED,    0, 15, },
    /* 16 */ {MB_TYPE_INTRA16x16, PLANE_PRED, 0, 15, },
    /* 17 */ {MB_TYPE_INTRA16x16, VERT_PRED,  1, 15, },
    /* 18 */ {MB_TYPE_INTRA16x16, HOR_PRED,   1, 15, },
    /* 19 */ {MB_TYPE_INTRA16x16, DC_PRED,    1, 15, },
    /* 20 */ {MB_TYPE_INTRA16x16, PLANE_PRED, 1, 15, },
    /* 21 */ {MB_TYPE_INTRA16x16, VERT_PRED,  2, 15, },
    /* 22 */ {MB_TYPE_INTRA16x16, HOR_PRED,   2, 15, },
    /* 23 */ {MB_TYPE_INTRA16x16, DC_PRED,    2, 15, },
    /* 24 */ {MB_TYPE_INTRA16x16, PLANE_PRED, 2, 15, },
    /* 25 */ {MB_TYPE_INTRA_PCM,  -1,        -1, -1, }
};


/* table 7-13 */
const P_MbInfo p_mb_type_info[5] = {
    /*  0 */{ MB_TYPE_16x16 | MB_TYPE_P0L0,                               1, 16, 16},
    /*  1 */{ MB_TYPE_16x8  | MB_TYPE_P0L0 | MB_TYPE_P1L0,                2, 16,  8},
    /*  2 */{ MB_TYPE_8x16  | MB_TYPE_P0L0 | MB_TYPE_P1L0,                2,  8, 16},
    /*  3 */{ MB_TYPE_8x8   | MB_TYPE_P0L0 | MB_TYPE_P1L0,                4,  8,  8},
    /*  4 */{ MB_TYPE_8x8   | MB_TYPE_P0L0 | MB_TYPE_P1L0 | MB_TYPE_REF0, 4,  8,  8},
};

/* table 7-17 */
const P_MbInfo p_sub_mb_type_info[4] = {
    /*  0 */ {SUB_MB_TYPE_8x8 | MB_TYPE_P0L0, 1, 8, 8},
    /*  1 */ {SUB_MB_TYPE_8x4 | MB_TYPE_P0L0, 2, 8, 4},
    /*  2 */ {SUB_MB_TYPE_4x8 | MB_TYPE_P0L0, 2, 4, 8},
    /*  3 */ {SUB_MB_TYPE_4x4 | MB_TYPE_P0L0, 4, 4, 4},
};



/* table 7-14 */
const B_MbInfo b_mb_type_info[23] = {
    /*  0 */{ MB_TYPE_DIRECT2 | MB_TYPE_L0L1,                                              1,  8,  8},
    /*  1 */{ MB_TYPE_16x16   | MB_TYPE_P0L0,                                              1, 16, 16},
    /*  2 */{ MB_TYPE_16x16   | MB_TYPE_P0L1,                                              1, 16, 16},
    /*  3 */{ MB_TYPE_16x16   | MB_TYPE_P0L0 | MB_TYPE_P0L1,                               1, 16, 16},
    /*  4 */{ MB_TYPE_16x8    | MB_TYPE_P0L0 | MB_TYPE_P1L0,                               2, 16, 16},
    /*  5 */{ MB_TYPE_8x16    | MB_TYPE_P0L0 | MB_TYPE_P1L0,                               2, 16,  8},
    /*  6 */{ MB_TYPE_16x8    | MB_TYPE_P0L1 | MB_TYPE_P1L1,                               2,  8, 16},
    /*  7 */{ MB_TYPE_8x16    | MB_TYPE_P0L1 | MB_TYPE_P1L1,                               2, 16,  8},
    /*  8 */{ MB_TYPE_16x8    | MB_TYPE_P0L0 | MB_TYPE_P1L1,                               2,  8, 16},
    /*  9 */{ MB_TYPE_8x16    | MB_TYPE_P0L0 | MB_TYPE_P1L1,                               2, 16,  8},
    /* 10 */{ MB_TYPE_16x8    | MB_TYPE_P0L1 | MB_TYPE_P1L0,                               2,  8, 16},
    /* 11 */{ MB_TYPE_8x16    | MB_TYPE_P0L1 | MB_TYPE_P1L0,                               2, 16,  8},
    /* 12 */{ MB_TYPE_16x8    | MB_TYPE_P0L0 | MB_TYPE_P1L0 | MB_TYPE_P1L1,                2,  8, 16},
    /* 13 */{ MB_TYPE_8x16    | MB_TYPE_P0L0 | MB_TYPE_P1L0 | MB_TYPE_P1L1,                2, 16,  8},
    /* 14 */{ MB_TYPE_16x8    | MB_TYPE_P0L1 | MB_TYPE_P1L0 | MB_TYPE_P1L1,                2,  8, 16},
    /* 15 */{ MB_TYPE_8x16    | MB_TYPE_P0L1 | MB_TYPE_P1L0 | MB_TYPE_P1L1,                2, 16,  8},
    /* 16 */{ MB_TYPE_16x8    | MB_TYPE_P0L0 | MB_TYPE_P0L1 | MB_TYPE_P1L0,                2,  8, 16},
    /* 17 */{ MB_TYPE_8x16    | MB_TYPE_P0L0 | MB_TYPE_P0L1 | MB_TYPE_P1L0,                2, 16,  8},
    /* 18 */{ MB_TYPE_16x8    | MB_TYPE_P0L0 | MB_TYPE_P0L1 | MB_TYPE_P1L1,                2,  8, 16},
    /* 19 */{ MB_TYPE_8x16    | MB_TYPE_P0L0 | MB_TYPE_P0L1 | MB_TYPE_P1L1,                2, 16,  8},
    /* 20 */{ MB_TYPE_16x8    | MB_TYPE_P0L0 | MB_TYPE_P0L1 | MB_TYPE_P1L0 | MB_TYPE_P1L1, 2,  8, 16},
    /* 21 */{ MB_TYPE_8x16    | MB_TYPE_P0L0 | MB_TYPE_P0L1 | MB_TYPE_P1L0 | MB_TYPE_P1L1, 2, 16,  8},
    /* 22 */{ MB_TYPE_8x8     | MB_TYPE_P0L0 | MB_TYPE_P0L1 | MB_TYPE_P1L0 | MB_TYPE_P1L1, 4,  8,  8},
};


/* table 7-18 */
const B_MbInfo b_sub_mb_type_info[13] = {
    /*  0 */ {SUB_MB_TYPE_DIRECT | SUB_MB_TYPE_8x8 | PRED_DIRECT,    4, 4, 4},
    /*  1 */ {SUB_MB_TYPE_8x8    | MB_TYPE_P0L0,                     1, 8, 8},
    /*  2 */ {SUB_MB_TYPE_8x8    | MB_TYPE_P0L1,                     1, 8, 8},
    /*  3 */ {SUB_MB_TYPE_8x8    | MB_TYPE_P0L0 | MB_TYPE_P0L1,      1, 8, 8},
    /*  4 */ {SUB_MB_TYPE_8x4    | MB_TYPE_P0L0,                     2, 8, 4},
    /*  5 */ {SUB_MB_TYPE_4x8    | MB_TYPE_P0L0,                     2, 4, 8},
    /*  6 */ {SUB_MB_TYPE_8x4    | MB_TYPE_P0L1,                     2, 8, 4},
    /*  7 */ {SUB_MB_TYPE_4x8    | MB_TYPE_P0L1,                     2, 4, 8},
    /*  8 */ {SUB_MB_TYPE_8x4    | MB_TYPE_P0L0 | MB_TYPE_P0L1,      2, 8, 4},
    /*  9 */ {SUB_MB_TYPE_4x8    | MB_TYPE_P0L0 | MB_TYPE_P0L1,      2, 4, 8},
    /* 10 */ {SUB_MB_TYPE_4x4    | MB_TYPE_P0L0,                     4, 4, 4},
    /* 11 */ {SUB_MB_TYPE_4x4    | MB_TYPE_P0L1,                     4, 4, 4},
    /* 12 */ {SUB_MB_TYPE_4x4    | MB_TYPE_P0L0 | MB_TYPE_P0L1,      4, 4, 4},

};


/* table 6-1 */
const int sub_width_c_info[4]  = {
    -1, /* monochrome */
     2, /*   4:2:0    */
     2, /*   4:2:2    */
     1, /*   4:4:4    */
};
const int sub_height_c_info[4] = {-1, 2, 1, 1};


/* table 6-2 */
const int luma_location_diff[4][2] = {
    /* N       {xD, yD} */

    /* A */ {-1,  0},
    /* B */ { 0, -1},
    /* C */ {-2, -1},
    /* D */ {-1, -1},
};


/* 7.3.5 */
void decode_macroblock(Macroblock *mb_array, int mbAddr, SliceHeader *sh, NalUnit *nal_unit, CodecContext *ctx) {
    BitReader *br = ctx->br;


    Macroblock *mb = &mb_array[mbAddr];

    PPS *pps = sh->pps;
    SPS *sps = sh->sps;


    print_macroblock_header(sh->sps->pic_order_cnt_type, mbAddr, sh->frame_num, sh->slice_type);


    print_slice_line_info(ctx->global_bit_offset, "mb_type", ctx->br);
    uint32_t mb_type = read_ue(br);
    print_slice_line_value(mb_type);


    if ((IS_I_SLICE(sh->slice_type) && mb_type > 25) ||
        (IS_P_SLICE(sh->slice_type) && mb_type > 4)  ||
        (IS_B_SLICE(sh->slice_type) && mb_type > 22)) {

        printf("mb_type out of bounds for %s slice : got %d\n",
            slice_type_to_string(sh->slice_type), mb_type);
        return;
    }

    int MbWidthC  = 16 / sub_width_c_info[sh->sps->chroma_format_idc];
    int MbHeightC = 16 / sub_height_c_info[sh->sps->chroma_format_idc];


    uint8_t pcm_samples_luma[256];
    uint8_t pcm_samples_chroma[2 * MbHeightC * MbWidthC];


    int type = IS_I_SLICE(sh->slice_type)
        ? i_mb_type_info[mb_type].type
        : IS_P_SLICE(sh->slice_type)
            ? p_mb_type_info[mb_type].type
            : IS_B_SLICE(sh->slice_type)
                ? b_mb_type_info[mb_type].type
                : -1;

    mb->type = type;

    int pred_mode = IS_I_SLICE(sh->slice_type)
        ? i_mb_type_info[mb_type].pred_mode
        : -1;

    int cbp_luma = IS_I_SLICE(sh->slice_type)
        ? i_mb_type_info[mb_type].cbp_luma
        : -1;

    int cbp_chroma = IS_I_SLICE(sh->slice_type)
        ? i_mb_type_info[mb_type].cbp_chroma
        : -1;



    if (type == MB_TYPE_INTRA_PCM) {
        while (!bitreader_byte_aligned(br)) {
            bitreader_skip_bits(br, 1);

        }
        for (int i = 0; i < 256; i++) {
            pcm_samples_luma[i] = read_u(br, 8);
        }
        for (int i = 0; i < 2 * MbHeightC * MbWidthC; i++) {
            pcm_samples_chroma[i] = read_u(br, 8);
        }
    } else {
        int transform_size_8x8_flag = 0;
        int noSubMbPartSizeLessThan8x8Flag = 1;
        if (type != MB_TYPE_INTRA4x4 && type != MB_TYPE_INTRA16x16) {

        } else {
            if (pps->transform_8x8_mode_flag && type == MB_TYPE_INTRA4x4) {
                transform_size_8x8_flag = read_u(br, 1);
            }
            mb_pred(mb_array, mbAddr, type, pred_mode, sh, nal_unit, ctx);
        }

        if (!IS_INTRA16x16(type)) {
            int32_t cbp = map_coded_block_pattern(read_ue(br), sps->chroma_format_idc,
                IS_INTRA4x4(type));

            cbp_luma   = cbp % 16;
            cbp_chroma = cbp / 16;

            if (cbp_luma > 0 && transform_size_8x8_flag &&
                !IS_INTRA4x4(type) && noSubMbPartSizeLessThan8x8Flag &&
                (type != (MB_TYPE_DIRECT2 | MB_TYPE_L0L1) || sps->direct_8x8_inference_flag)) {

                transform_size_8x8_flag = read_u(br, 1);
            }
        }

        if (cbp_luma > 0 || cbp_chroma > 0 || IS_INTRA16x16(type)) {
            print_slice_line_info(ctx->global_bit_offset, "mb_qp_delta", ctx->br);
            mb->mb_qp_delta = read_se(br);
            print_slice_line_value(mb->mb_qp_delta);

            residual_function residual_block = sh->pps->entropy_coding_mode_flag
                ? &residual_block_cabac
                : &residual_block_cavlc;


            residual(mb_array, mbAddr, type, transform_size_8x8_flag, 0, 15, cbp_luma, cbp_chroma,
                        residual_block, sh, ctx);
        }
    };
}


/* 7.3.5.1 */
void mb_pred(Macroblock *mb_array, int mbAddr, int type, int pred_mode, SliceHeader *sh, NalUnit *nal_unit, CodecContext *ctx) {
    BitReader *br = ctx->br;


    if (type == MB_TYPE_INTRA4x4 ||
        type == MB_TYPE_INTRA8x8 ||
        type == MB_TYPE_INTRA16x16) {

        if (type == MB_TYPE_INTRA4x4) {
            uint8_t prev_intra4x4_pred_mode_flag[16];
            uint8_t rem_intra4x4_pred_mode[16];
            for (int luma4x4BlkIdx = 0; luma4x4BlkIdx < 16; luma4x4BlkIdx++) {
                prev_intra4x4_pred_mode_flag[luma4x4BlkIdx] = read_u(br, 1);
                if (!prev_intra4x4_pred_mode_flag[luma4x4BlkIdx]) {
                    rem_intra4x4_pred_mode[luma4x4BlkIdx] = read_u(br, 3);
                }
            }
        }

        if (type == MB_TYPE_INTRA8x8) {
            uint8_t prev_intra8x8_pred_mode_flag[16];
            uint8_t rem_intra8x8_pred_mode[16];
            for (int luma8x8BlkIdx = 0; luma8x8BlkIdx < 4; luma8x8BlkIdx++) {
                prev_intra8x8_pred_mode_flag[luma8x8BlkIdx] = read_u(br, 1);
                if (!prev_intra8x8_pred_mode_flag[luma8x8BlkIdx]) {
                    rem_intra8x8_pred_mode[luma8x8BlkIdx] = read_u(br, 3);
                }
            }
        }

        if (sh->sps->chroma_format_idc == 1 || sh->sps->chroma_format_idc == 2) {
            print_slice_line_info(ctx->global_bit_offset, "intra_chroma_pred_mode", ctx->br);
            mb_array[mbAddr].intra_chroma_pred_mode = read_ue(br);
            print_slice_line_value((int32_t)mb_array[mbAddr].intra_chroma_pred_mode);
        }

    } else if (type != MB_TYPE_DIRECT2) {

    }
}


/* 7.3.5.3 */
void residual(Macroblock *mb_array, int mbAddr, int type, int t_8x8_flag, int startIdx, int endIdx, int cbp_luma, int cbp_chroma, residual_function residual_block, SliceHeader *sh, CodecContext *ctx) {
    BitReader *br = ctx->br;

    Macroblock *mb = &mb_array[mbAddr];

    residual_luma(mb_array, mbAddr, type, t_8x8_flag, cbp_luma, startIdx, endIdx,
                    residual_block, sh, ctx);



    if (sh->sps->chroma_format_idc == 1 || sh->sps->chroma_format_idc == 2) {

        int numC8x8 = 4 /
            (sub_width_c_info[sh->sps->chroma_format_idc] * sub_height_c_info[sh->sps->chroma_format_idc]);


        for (int iCbCr = 0; iCbCr < 2; iCbCr++) {
            if ((cbp_chroma & 3) && startIdx == 0) {
                /* chroma DC residual present */
                (*residual_block)(mb_array, mbAddr, 0, CHROMA_DC_LEVEL, mb->residuals.chroma_DC[iCbCr], 0, 4*numC8x8-1, 4*numC8x8, sh, ctx);
            } else {
                for (int i = 0; i < 4 * numC8x8; i++) {
                    mb->residuals.chroma_DC[iCbCr][i] = 0;
                }
            }
        }

        for (int iCbCr = 0; iCbCr < 2; iCbCr++) {
            for (int i8x8 = 0; i8x8 < numC8x8; i8x8++) {
                for (int i4x4 = 0; i4x4 < 4; i4x4++) {
                    if (cbp_chroma & 2) {
                        /* chroma AC residual present */
                        (*residual_block)(mb_array, mbAddr, i4x4, CHROMA_AC_LEVEL, mb->residuals.chroma_AC[iCbCr][i8x8*4 + i4x4],
                            _max(0, startIdx-1), endIdx-1, 15, sh, ctx);
                    } else {
                        for (int i = 0; i < 15; i++) {
                            mb->residuals.chroma_AC[iCbCr][i8x8*4 + i4x4][i] = 0;
                        }
                    }
                }
            }
        }
    } else if (sh->sps->chroma_format_idc == 3) { /* 4:4:4 not handled for now */
        int16_t CbIntra16x16DC[16];
        int16_t CbIntra16x16AC[16][15];
        int16_t Cb4x4[16][16];
        int16_t Cb8x8[4][64];
        residual_luma(mb_array, mbAddr, type, t_8x8_flag, cbp_chroma, startIdx,  endIdx,
                         residual_block, sh, ctx);

        int16_t CrIntra16x16DC[16];
        int16_t CrIntra16x16AC[16][15];
        int16_t Cr4x4[16][16];
        int16_t Cr8x8[4][64];
        residual_luma(mb_array, mbAddr, type, t_8x8_flag, cbp_chroma, startIdx,  endIdx,
                        residual_block, sh, ctx);
    }
}



/* 7.3.5.3.1 */
void residual_luma(Macroblock *mb_array, int mbAddr, int type, int t_8x8_flag, int cbp_luma,
                    int startIdx, int endIdx,
                    residual_function residual_block, SliceHeader *sh, CodecContext *ctx) {

    BitReader *br = ctx->br;


    Macroblock *mb = &mb_array[mbAddr];


    if (startIdx == 0 && IS_INTRA16x16(type)) {
        (*residual_block)(mb_array, mbAddr, 0, LUMA_INTRA_16x16_DC_LEVEL, mb->residuals.luma_16x16_DC, 0, 15, 16, sh, ctx);
    }

    for (int i8x8 = 0; i8x8 < 4; i8x8++) {
        if (!t_8x8_flag || !sh->pps->entropy_coding_mode_flag) {
            for (int i4x4 = 0; i4x4 < 4; i4x4++) {
                if (cbp_luma & (1 << i8x8)) {
                    if (IS_INTRA16x16(type)) {
                        (*residual_block)(mb_array, mbAddr, i8x8*4 + i4x4, LUMA_INTRA_16x16_AC_LEVEL, mb->residuals.luma_16x16_AC[i8x8*4 + i4x4],
                            _max(0, startIdx - 1), endIdx - 1, 15, sh, ctx);
                    } else {
                        (*residual_block)(mb_array, mbAddr, i8x8*4 + i4x4, LUMA_LEVEL_4x4, mb->residuals.luma_4x4_coeffs[i8x8*4 + i4x4],
                            startIdx, endIdx, 16, sh, ctx);
                    }
                } else if (IS_INTRA16x16(type)) {
                    for (int i = 0; i < 15; i++) {
                        mb->residuals.luma_16x16_AC[i8x8*4 + i4x4][i] = 0;
                    }
                } else {
                    for (int i = 0; i < 16; i++) {
                        mb->residuals.luma_4x4_coeffs[i8x8*4 + i4x4][i] = 0;
                    }
                }

                if (!sh->pps->entropy_coding_mode_flag && t_8x8_flag) {
                    for (int i = 0; i < 16; i++) {
                        mb->residuals.luma_8x8_coeffs[i8x8][4*i + i4x4] = mb->residuals.luma_4x4_coeffs[i8x8*4 + i4x4][i];
                    }
                }
            }
        } else if (cbp_luma & (1 << i8x8)) {
            (*residual_block)(mb_array, mbAddr, 0, LUMA_LEVEL_8x8, mb->residuals.luma_8x8_coeffs[i8x8], 4*startIdx, 4*endIdx+3, 64, sh, ctx);
        } else {
            for (int i = 0; i < 64; i++) {
                mb->residuals.luma_8x8_coeffs[i8x8][i] = 0;
            }
        }
    }
}



/* 7.3.5.3.3 */
void residual_block_cabac(Macroblock *mb_array, int mbAddr, int blkIdx, int pbt, int16_t coeffLevel[], int startIdx, int endIdx, int maxNumCoeff, SliceHeader *sh, CodecContext *ctx) {

}


void derive_neighbor_4x4_luma(int luma4x4BlkIdx, int currMbAddr, int *mbAddrA, int *luma4x4BlkIdxA, int *mbAddrB, int *luma4x4BlkIdxB, SliceHeader *sh) {
    /* mb A */
    int xD = luma_location_diff[0][0];
    int yD = luma_location_diff[0][1];

    Coord luma_4x4_pos = inverse_4x4_luma_blk_scan(luma4x4BlkIdx);

    int xA = luma_4x4_pos.x + xD;
    int yA = luma_4x4_pos.y + yD;

    Coord neighbor_pos = {xA, yA};
    Coord xWyW = {};
    derive_neighbor(1, neighbor_pos, currMbAddr, mbAddrA, &xWyW, sh);

    *luma4x4BlkIdxA = *mbAddrA != -1 ? derive_4x4_luma_blk(xWyW.x, xWyW.y) : -1;


    /* mb B */
    xD = luma_location_diff[1][0];
    yD = luma_location_diff[1][1];

    int xB = luma_4x4_pos.x + xD;
    int yB = luma_4x4_pos.y + yD;

    neighbor_pos.x = xB; neighbor_pos.y = yB;
    derive_neighbor(1, luma_4x4_pos, currMbAddr, mbAddrB, &xWyW, sh);

    *luma4x4BlkIdxB = *mbAddrB != -1 ? derive_4x4_luma_blk(xWyW.x, xWyW.y) : -1;
}

void derive_neighbor(bool luma, Coord location, int currMbAddr, int *mbAddrN, Coord *xWyW, SliceHeader *sh) {
    int max = luma ? 16 : 8; // FIXME: use MbWidthC and MbHeightC instead of 4:2:0 assumption for chroma

    if (location.x < 0) {
        *mbAddrN = currMbAddr - (location.y < 0 ? (int)sh->sps->pic_width_in_mbs : 1);
    } else if (location.x < max) {
        *mbAddrN = currMbAddr - (location.y < 0 ? (int)sh->sps->pic_width_in_mbs : 0);
    } else {
        *mbAddrN = location.y >= 0 ? currMbAddr - (int)sh->sps->pic_width_in_mbs + 1 : -1;
    }
    xWyW->x = (location.x + max) % max;
    xWyW->y = (location.y + max) % max;
}