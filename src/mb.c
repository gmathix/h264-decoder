//
// Created by gmathix on 3/21/26.
//

#include "cavlc.h"
#include "global.h"
#include "mb.h"
#include "util/expgolomb.h"
#include "util/formulas.h"
#include "util/mbutil.h"
#include "util/predutil.h"


#include <stdlib.h>
#include <stdio.h>

#include "intra.h"
#include "picture.h"
#include "transform.h"
#include "util/sliceutil.h"


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


const int QPcTable[52] = {
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
   10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
   20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
   29, 30, 31, 32, 32, 33, 34, 34, 35, 35,
   36, 36, 37, 37, 38, 38, 39, 39, 40, 40,
   39, 39
};
const uint8_t block_mapping_4x4[16] = {
    0,   1,  4,  5,
    2,   3,  6,  7,
    8,   9, 12, 13,
    10, 11, 14, 15,
};

/* table 6-2 */
const int luma_location_diff[4][2] = {
    /* N    {xD, yD} */

    /* A */ {-1,  0},
    /* B */ { 0, -1},
    /* C */ {-2, -1},
    /* D */ {-1, -1},
};



// macroblock neighbors for each 4x4 luma block
int16_t luma_4x4_blk_mb_neighbors[16][4] = {
    //  1 = right macroblock    0 = current macroblock    -1 = A neighbor
    // -2 = C neighbor         -3 = B neighbor            -4 = D neighbor
    // -5 = not available
    // see fig 6-14

    // {left, top, top right, top left}
    {-1, -3, -3, -4},    { 0, -3, -3, -3},    { 0, -3, -3, -3},    { 0, -3, -2, -3},
    {-1,  0,  0, -1},    { 0,  0, -5,  0},    { 0,  0,  0,  0},    { 0,  0, -5,  0},
    {-1,  0,  0, -1},    { 0,  0,  0,  0},    { 0,  0,  0,  0},    { 0,  0, -5,  0},
    {-1,  0,  0, -1},    { 0,  0, -5,  0},    { 0,  0,  0,  0},    { 0,  0, -5,  0}
};


// neighbor 4x4 luma block index (raster) inside neighbor macroblock
int8_t luma_4x4_blk_neighbor_idx[16][4] = {
    { 3, 12, 13, 15},    { 0, 13, 14, 12},    { 1, 14, 15, 13},    { 2, 15, 12, 14},
    { 7,  0,  1,  3},    { 4,  1, -1,  0},    { 5,  2,  3,  1},    { 6,  3, -1,  2},
    {11,  4,  5,  7},    { 8,  5,  6,  4},    { 9,  6,  7,  5},    {10,  7, -1,  6},
    {15,  8,  9, 11},    {12,  9, -1,  8},    {13, 10, 11,  9},    {14, 11, -1, 10}
};

// neighbor 4x4 luma block coords inside neighbor macroblock
int8_t luma_4x4_blk_neighbor_coords[16][4][2] = {
    // {y,x}
    { { 0,12},{12, 0},{12, 4},{12,12} },   { { 0, 0},{12, 4},{12, 8},{12, 0} },   { { 0, 4},{12, 8},{12,12},{12, 4} },   { { 0, 8},{12,12},{12, 0},{12, 8} },
    { { 4,12},{ 0, 0},{ 0, 4},{ 0,12} },   { { 4, 0},{ 0, 4},{ 0, 8},{ 0, 0} },   { { 4, 4},{ 0, 8},{ 0,12},{ 0, 4} },   { { 4, 8},{ 0,12},{ 0, 0},{ 0, 8} },
    { { 8,12},{ 4, 0},{ 4, 4},{ 4,12} },   { { 8, 0},{ 4, 4},{ 4, 8},{ 4, 0} },   { { 8, 4},{ 4, 8},{ 4,12},{ 4, 4} },   { { 8, 8},{ 4,12},{ 4, 0},{ 4, 8} },
    { {12,12},{ 8, 0},{ 8, 4},{ 8,12} },   { {12, 0},{ 8, 4},{ 8, 8},{ 8, 0} },   { {12, 4},{ 8, 8},{ 8,12},{ 8, 4} },   { {12, 8},{ 8,12},{ 8, 0},{ 8, 8} },
};


// macroblock neighbors for each 4x4 chroma block
int16_t chroma_4x4_blk_mb_neighbors[4][4] = {
    {-1, -3, -3, -4},   { 0, -3, -2, -3},
    {-1,  0,  0, -1},   { 0,  0, -5,  0}
};

// neighbor 4x4 chroma block index (raster) inside neighbor macroblock
int8_t chroma_4x4_blk_neighbor_idx[4][4] = {
    { 1,  2,  3,  3},   { 0,  3,  2,  2},
    { 3,  0,  1,  1},   { 2,  1, -1,  0},
};

// neighbor 4x4 chroma block coords inside neighbor macroblock
int8_t chroma_4x4_blk_neighbor_coords[4][4][2] = {
    { {0,4},{4,0},{4,4},{4,4} },   { {0,0},{4,4},{4,0},{4,0} },
    { {4,4},{0,0},{0,4},{0,4} },   { {4,0},{0,4},{0,0},{0,0} }
};




int neighbor_tables_initialized = 0;

void init_neighbor_tables(CodecContext *ctx) {
    // replace -3 by -mb_width, -2 by -mb_width+1 and -4 by -mb_width-1

    int mb_width = ctx->ps->sps->pic_width_in_mbs;
    for (int i = 0; i < 16; i++) {
        for (int m = 0; m < 4; m++) {
            if      (luma_4x4_blk_mb_neighbors[i][m] == -2) luma_4x4_blk_mb_neighbors[i][m] = -mb_width+1;
            else if (luma_4x4_blk_mb_neighbors[i][m] == -3) luma_4x4_blk_mb_neighbors[i][m] = -mb_width;
            else if (luma_4x4_blk_mb_neighbors[i][m] == -4) luma_4x4_blk_mb_neighbors[i][m] = -mb_width-1;
        }
    }
    for (int i = 0; i < 4; i++) {
        for (int m = 0; m < 4; m++) {
            if      (chroma_4x4_blk_mb_neighbors[i][m] == -2) chroma_4x4_blk_mb_neighbors[i][m] = -mb_width+1;
            else if (chroma_4x4_blk_mb_neighbors[i][m] == -3) chroma_4x4_blk_mb_neighbors[i][m] = -mb_width;
            else if (chroma_4x4_blk_mb_neighbors[i][m] == -4) chroma_4x4_blk_mb_neighbors[i][m] = -mb_width-1;
        }
    }

    neighbor_tables_initialized = 1;
}



/* 7.3.5 */
void read_macroblock(Macroblock *mb_array, int mbAddr, SliceHeader *sh, NalUnit *nal_unit, CodecContext *ctx) {
    BitReader *br = ctx->br;


    Macroblock *mb = &mb_array[mbAddr];

    PPS *pps = sh->pps;
    SPS *sps = sh->sps;


#if NAL_LOG
    print_macroblock_header(sh->sps->pic_order_cnt_type, mbAddr, sh->frame_num, sh->slice_type);
#endif


#if NAL_LOG
    print_slice_line_info(ctx->global_bit_offset, "mb_type", ctx->br);
#endif
    uint32_t mb_type = read_ue(br);
    mb->table_idx = mb_type;
#if NAL_LOG
    print_slice_line_value(mb_type);
#endif



    if ((IS_I_SLICE(sh->slice_type) && mb_type > 25) ||
        (IS_P_SLICE(sh->slice_type) && mb_type > 4)  ||
        (IS_B_SLICE(sh->slice_type) && mb_type > 22)) {

        printf("mb_type out of bounds for %s slice : got %d\n",
            slice_type_to_string(sh->slice_type), mb_type);
        return;
    }

    mb->mb_height_c = 16 / sub_height_c_info[sh->sps->chroma_format_idc];
    mb->mb_width_c  = 16 / sub_width_c_info[sh->sps->chroma_format_idc];

    mb->QPY = mbAddr == 0
                ? sh->pps->pic_init_qp + sh->slice_qp_delta
                : mb_array[mbAddr-1].QPY;


    uint8_t pcm_samples_luma[256];
    uint8_t pcm_samples_chroma[2 * mb->mb_height_c * mb->mb_width_c];


    int type = IS_I_SLICE(sh->slice_type)
        ? i_mb_type_info[mb_type].type
        : IS_P_SLICE(sh->slice_type)
            ? p_mb_type_info[mb_type].type
            : IS_B_SLICE(sh->slice_type)
                ? b_mb_type_info[mb_type].type
                : -1;

    mb->mb_type = type;


    if (IS_I_SLICE(sh->slice_type)) mb->pred_mode = i_mb_type_info[mb_type].pred_mode;
    else mb->pred_mode = -1;


    mb->slice_type = sh->slice_type;

    int pred_mode = IS_I_SLICE(sh->slice_type)
        ? i_mb_type_info[mb_type].pred_mode
        : -1;

    int cbp_luma = IS_I_SLICE(sh->slice_type)
        ? i_mb_type_info[mb_type].cbp_luma
        : -1;

    int cbp_chroma = IS_I_SLICE(sh->slice_type)
        ? i_mb_type_info[mb_type].cbp_chroma
        : -1;

    mb->residuals.cbp_chroma = cbp_chroma;
    mb->residuals.cbp_luma = cbp_luma;



    if (type == MB_TYPE_INTRA_PCM) {
        while (!bitreader_byte_aligned(br)) {
            bitreader_skip_bits(br, 1);

        }
        for (int i = 0; i < 256; i++) {
            pcm_samples_luma[i] = read_u(br, 8);
        }
        for (int i = 0; i < 2 * mb->mb_height_c * mb->mb_width_c; i++) {
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
            read_mb_pred(mb_array, mbAddr, type, pred_mode, sh, nal_unit, ctx);
        }

        if (!IS_INTRA16x16(type)) {
            int32_t cbp = map_coded_block_pattern(read_ue(br), sps->chroma_format_idc,
                IS_INTRA4x4(type));

            cbp_luma   = cbp % 16;
            cbp_chroma = cbp / 16;
            mb->residuals.cbp_chroma = cbp_chroma;
            mb->residuals.cbp_luma = cbp_luma;

            if (cbp_luma > 0 && transform_size_8x8_flag &&
                !IS_INTRA4x4(type) && noSubMbPartSizeLessThan8x8Flag &&
                (type != (MB_TYPE_DIRECT2 | MB_TYPE_L0L1) || sps->direct_8x8_inference_flag)) {

                transform_size_8x8_flag = read_u(br, 1);
            }
        }

        if (cbp_luma > 0 || cbp_chroma > 0 || IS_INTRA16x16(type)) {
#if NAL_LOG
            print_slice_line_info(ctx->global_bit_offset, "mb_qp_delta", ctx->br);
#endif
            mb->mb_qp_delta = read_se(br);
#if NAL_LOG
            print_slice_line_value(mb->mb_qp_delta);
#endif


            mb->QPY = mbAddr == 0
                ? _clip3(0, 51, (pps->pic_init_qp + sh->slice_qp_delta + mb->mb_qp_delta + 52) % 52)
                : _clip3(0, 51, (mb_array[mbAddr-1].QPY + mb->mb_qp_delta + 52) % 52);


            int qPi = _clip3(0, 51, mb->QPY + pps->chroma_qp_index_offset);
            mb->QPC = QPcTable[qPi];

            residual_func residual_block = sh->pps->entropy_coding_mode_flag
                ? &residual_block_cabac
                : &residual_block_cavlc;


            read_residual(mb_array, mbAddr, type, transform_size_8x8_flag, 0, 15, cbp_luma, cbp_chroma,
                        residual_block, sh, ctx);
        } else {
            mb->QPY = mb_array[mbAddr-1].QPY;
            mb->QPC = mb_array[mbAddr-1].QPC;
        }
    }
}


/* 7.3.5.1 */
void read_mb_pred(Macroblock *mb_array, int mbAddr, int type, int pred_mode, SliceHeader *sh, NalUnit *nal_unit, CodecContext *ctx) {
    BitReader *br = ctx->br;
    Macroblock *mb = &mb_array[mbAddr];


    if (type == MB_TYPE_INTRA4x4 ||
        type == MB_TYPE_INTRA8x8 ||
        type == MB_TYPE_INTRA16x16) {

        if (type == MB_TYPE_INTRA4x4) {
            int intraModeA, intraModeB;
            for (int i = 0; i < 16; i++) {
                int blkIdx = block_mapping_4x4[i];

                Neighbors n = derive_neighbors_4x4_luma(mb, blkIdx, ctx);

                int dcPredModePredictedFlag;
                if (!n.a_av || !n.b_av ||
                    (n.a_av && IS_INTER(n.mb_a->mb_type) && ctx->ps->pps->constrained_intra_pred_flag) ||
                    (n.b_av && IS_INTER(n.mb_b->mb_type) && ctx->ps->pps->constrained_intra_pred_flag)) {
                    dcPredModePredictedFlag = 1;
                } else {
                    dcPredModePredictedFlag = 0;
                }

                if (dcPredModePredictedFlag || !IS_INTRANxN(n.mb_a->mb_type)) {
                    intraModeA = DC_PRED;
                } else {
                    intraModeA = IS_INTRA4x4(n.mb_a->mb_type)
                        ? n.mb_a->intra4x4_pred_mode[n.a_idx]
                        : n.mb_a->intra8x8_pred_mode[n.a_idx];
                }
                if (dcPredModePredictedFlag || !IS_INTRANxN(n.mb_b->mb_type)) {
                    intraModeB = DC_PRED;
                } else {
                    intraModeB = IS_INTRA4x4(n.mb_b->mb_type)
                        ? n.mb_b->intra4x4_pred_mode[n.b_idx]
                        : n.mb_b->intra8x8_pred_mode[n.b_idx];
                }

                int predIntraMode = _min(intraModeA, intraModeB);

                uint8_t prev_intra4x4_pred_mode_flag = read_u(br, 1);
                if (!prev_intra4x4_pred_mode_flag) {
                    uint8_t rem_intra4x4_pred_mode = read_u(br, 3);
                    mb->intra4x4_pred_mode[blkIdx] = rem_intra4x4_pred_mode < predIntraMode
                        ? rem_intra4x4_pred_mode
                        : rem_intra4x4_pred_mode+1;
                } else {
                    mb->intra4x4_pred_mode[blkIdx] = predIntraMode;
                }
            }
        }

        if (type == MB_TYPE_INTRA8x8) {
            for (int luma8x8BlkIdx = 0; luma8x8BlkIdx < 4; luma8x8BlkIdx++) {
                uint8_t prev_intra8x8_pred_mode_flag = read_u(br, 1);
                if (!prev_intra8x8_pred_mode_flag) {
                    uint8_t rem_intra8x8_pred_mode = read_u(br, 3);
                }
            }
        }

        if (sh->sps->chroma_format_idc == 1 || sh->sps->chroma_format_idc == 2) {
#if NAL_LOG
            print_slice_line_info(ctx->global_bit_offset, "intra_chroma_pred_mode", ctx->br);
#endif
            mb_array[mbAddr].intra_chroma_pred_mode = read_ue(br);
#if NAL_LOG
            print_slice_line_value((int32_t)mb_array[mbAddr].intra_chroma_pred_mode);
#endif
        }

    } else if (type != MB_TYPE_DIRECT2) {

    }
}


/* 7.3.5.3 */

void read_residual(Macroblock *mb_array, int mbAddr, int type, int t_8x8_flag, int startIdx, int endIdx, int cbp_luma, int cbp_chroma, residual_func residual_block, SliceHeader *sh, CodecContext *ctx) {
    BitReader *br = ctx->br;

    Macroblock *mb = &mb_array[mbAddr];

    read_residual_luma(mb_array, mbAddr, type, t_8x8_flag, cbp_luma, startIdx, endIdx,
                    residual_block, sh, ctx);



    if (sh->sps->chroma_format_idc == 1 || sh->sps->chroma_format_idc == 2) {

        int numC8x8 = 4 /
            (sub_width_c_info[sh->sps->chroma_format_idc] * sub_height_c_info[sh->sps->chroma_format_idc]);


        for (int iCbCr = 0; iCbCr < 2; iCbCr++) {
            uint8_t (*chroma_coeff_table)[16] = iCbCr
                ? mb->p_frame->cr_total_coeffs
                : mb->p_frame->cb_total_coeffs;

            if ((cbp_chroma & 3) && startIdx == 0) {
                /* chroma DC residual present */
                (*residual_block)(mb, 0, iCbCr, CHROMA_DC_LEVEL, mb->residuals.chroma_DC[iCbCr], chroma_coeff_table, 0, 4*numC8x8-1, 4*numC8x8, false, sh, ctx);
            } else {
                for (int i = 0; i < 4 * numC8x8; i++) {
                    mb->residuals.chroma_DC[iCbCr][i] = 0;
                }
            }
        }

        for (int iCbCr = 0; iCbCr < 2; iCbCr++) {
            uint8_t (*chroma_coeff_table)[16] = iCbCr
                ? mb->p_frame->cr_total_coeffs
                : mb->p_frame->cb_total_coeffs;

            for (int i8x8 = 0; i8x8 < numC8x8; i8x8++) {
                for (int i4x4 = 0; i4x4 < 4; i4x4++) {
                    if (cbp_chroma & 2) {
                        /* chroma AC residual present */
                        (*residual_block)(mb, i4x4, iCbCr, CHROMA_AC_LEVEL, mb->residuals.chroma_AC[iCbCr][i8x8*4 + i4x4], chroma_coeff_table,
                            _max(0, startIdx-1), endIdx-1, 15, false, sh, ctx);
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
        read_residual_luma(mb_array, mbAddr, type, t_8x8_flag, cbp_chroma, startIdx,  endIdx,
                         residual_block, sh, ctx);

        int16_t CrIntra16x16DC[16];
        int16_t CrIntra16x16AC[16][15];
        int16_t Cr4x4[16][16];
        int16_t Cr8x8[4][64];
        read_residual_luma(mb_array, mbAddr, type, t_8x8_flag, cbp_chroma, startIdx,  endIdx,
                        residual_block, sh, ctx);
    }
}



/* 7.3.5.3.1 */
void read_residual_luma(Macroblock *mb_array, int mbAddr, int type, int t_8x8_flag, int cbp_luma,
                    int startIdx, int endIdx,
                    residual_func residual_block, SliceHeader *sh, CodecContext *ctx) {

    BitReader *br = ctx->br;


    Macroblock *mb = &mb_array[mbAddr];


    if (startIdx == 0 && IS_INTRA16x16(type)) {
        (*residual_block)(mb, 0, 0, LUMA_INTRA_16x16_DC_LEVEL, mb->residuals.luma_16x16_DC, mb->p_frame->luma_total_coeffs, 0, 15, 16, true, sh, ctx);
    }

    for (int i8x8 = 0; i8x8 < 4; i8x8++) {
        if (!t_8x8_flag || !sh->pps->entropy_coding_mode_flag) {
            for (int i4x4 = 0; i4x4 < 4; i4x4++) {
                int blkIdx = block_mapping_4x4[i8x8*4+i4x4];
                if (cbp_luma & (1 << i8x8)) {
                    if (IS_INTRA16x16(type)) {
                        (*residual_block)(mb, blkIdx, 0, LUMA_INTRA_16x16_AC_LEVEL, mb->residuals.luma_16x16_AC[blkIdx], mb->p_frame->luma_total_coeffs,
                            _max(0, startIdx - 1), endIdx - 1, 15, true, sh, ctx);
                    } else {
                        (*residual_block)(mb, blkIdx, 0, LUMA_LEVEL_4x4, mb->residuals.luma_4x4_coeffs[blkIdx], mb->p_frame->luma_total_coeffs,
                            startIdx, endIdx, 16, true, sh, ctx);
                    }
                } else if (IS_INTRA16x16(type)) {
                    for (int i = 0; i < 15; i++) {
                        mb->residuals.luma_16x16_AC[blkIdx][i] = 0;
                    }
                } else {
                    for (int i = 0; i < 16; i++) {
                        mb->residuals.luma_4x4_coeffs[blkIdx][i] = 0;
                    }
                }

                if (!sh->pps->entropy_coding_mode_flag && t_8x8_flag) {
                    for (int i = 0; i < 16; i++) {
                        mb->residuals.luma_8x8_coeffs[i8x8][4*i + i4x4] = mb->residuals.luma_4x4_coeffs[i8x8*4 + i4x4][i];
                    }
                }
            }
        } else if (cbp_luma & (1 << i8x8)) {
            (*residual_block)(mb, 0, 0, LUMA_LEVEL_8x8, mb->residuals.luma_8x8_coeffs[i8x8], mb->p_frame->luma_total_coeffs,
                4*startIdx, 4*endIdx+3, 64, true, sh, ctx);
        } else {
            for (int i = 0; i < 64; i++) {
                mb->residuals.luma_8x8_coeffs[i8x8][i] = 0;
            }
        }
    }
}



/* 7.3.5.3.3 */
void residual_block_cabac(Macroblock *mb, int blkIdx, int iCbCr, int pbt, int16_t coeffLevel[], uint8_t (*total_coeffs_table)[16],
    int startIdx, int endIdx, int maxNumCoeff, bool isLuma, SliceHeader *sh, CodecContext *ctx) {

}



void decode_i_macroblock(Macroblock *mb, Slice *slice, CodecContext *ctx) {
    if (IS_INTRA4x4(mb->mb_type)) {
        for (int i = 0; i < 16; i++) {
            int blkIdx = block_mapping_4x4[i];
            intra_pred_4x4(mb, blkIdx, mb->intra4x4_pred_mode[blkIdx], ctx);
            transform_luma_4x4(mb, mb->QPY, blkIdx, ctx);
        }

        intra_chroma_pred(mb, ctx);
        transform_chroma(mb, ctx);

    } else if (IS_INTRA16x16(mb->mb_type)) {
        intra_pred_16x16(mb, ctx);
        intra_chroma_pred(mb, ctx);

        transform_luma_16x16(mb, mb->QPY, ctx);
        transform_chroma(mb, ctx);

    } else if (IS_INTRA8x8(mb->mb_type)) {

    } else if (IS_INTRA_PCM(mb->mb_type)) {

    }
}


void decode_p_macroblock(Macroblock *mb, Slice *slice, CodecContext *ctx) {

}

void decode_b_macroblock(Macroblock *mb,  Slice *slice, CodecContext *ctx) {

}