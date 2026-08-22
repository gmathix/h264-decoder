//
// Created by gmathix on 3/21/26.
//


#include "mb.h"


#include "cavlc.h"
#include "inter.h"
#include "intra.h"
#include "mvpred.h"
#include "transform.h"

#include "tests/profiler.h"
#include "util/sliceutil.h"
#include "util/expgolomb.h"
#include "util/mbutil.h"
#include "util/predutil.h"
#include "util/logger.h"

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
const PB_MbInfo p_mb_type_info[5] = {
    /*  0 */{ MB_TYPE_16x16 | MB_TYPE_P0L0,                               1, 16, 16},
    /*  1 */{ MB_TYPE_16x8  | MB_TYPE_P0L0 | MB_TYPE_P1L0,                2, 16,  8},
    /*  2 */{ MB_TYPE_8x16  | MB_TYPE_P0L0 | MB_TYPE_P1L0,                2,  8, 16},
    /*  3 */{ MB_TYPE_8x8   | MB_TYPE_P0L0 | MB_TYPE_P1L0,                4,  8,  8},
    /*  4 */{ MB_TYPE_8x8   | MB_TYPE_P0L0 | MB_TYPE_P1L0 | MB_TYPE_REF0, 4,  8,  8},
};

/* table 7-17 */
const PB_MbInfo p_sub_mb_type_info[4] = {
    /*  0 */ {SUB_MB_TYPE_8x8 | MB_TYPE_P0L0, 1, 8, 8},
    /*  1 */ {SUB_MB_TYPE_8x4 | MB_TYPE_P0L0, 2, 8, 4},
    /*  2 */ {SUB_MB_TYPE_4x8 | MB_TYPE_P0L0, 2, 4, 8},
    /*  3 */ {SUB_MB_TYPE_4x4 | MB_TYPE_P0L0, 4, 4, 4},
};



/* table 7-14 */
const PB_MbInfo b_mb_type_info[23] = {
    /*  0 */{ MB_TYPE_DIRECT  | MB_TYPE_L0L1,                                              1,  8,  8},
    /*  1 */{ MB_TYPE_16x16   | MB_TYPE_P0L0,                                              1, 16, 16},
    /*  2 */{ MB_TYPE_16x16   | MB_TYPE_P0L1,                                              1, 16, 16},
    /*  3 */{ MB_TYPE_16x16   | MB_TYPE_P0L0 | MB_TYPE_P0L1,                               1, 16, 16},
    /*  4 */{ MB_TYPE_16x8    | MB_TYPE_P0L0 | MB_TYPE_P1L0,                               2, 16,  8},
    /*  5 */{ MB_TYPE_8x16    | MB_TYPE_P0L0 | MB_TYPE_P1L0,                               2,  8, 16},
    /*  6 */{ MB_TYPE_16x8    | MB_TYPE_P0L1 | MB_TYPE_P1L1,                               2, 16,  8},
    /*  7 */{ MB_TYPE_8x16    | MB_TYPE_P0L1 | MB_TYPE_P1L1,                               2,  8, 16},
    /*  8 */{ MB_TYPE_16x8    | MB_TYPE_P0L0 | MB_TYPE_P1L1,                               2, 16,  8},
    /*  9 */{ MB_TYPE_8x16    | MB_TYPE_P0L0 | MB_TYPE_P1L1,                               2,  8, 16},
    /* 10 */{ MB_TYPE_16x8    | MB_TYPE_P0L1 | MB_TYPE_P1L0,                               2, 16,  8},
    /* 11 */{ MB_TYPE_8x16    | MB_TYPE_P0L1 | MB_TYPE_P1L0,                               2,  8, 16},
    /* 12 */{ MB_TYPE_16x8    | MB_TYPE_P0L0 | MB_TYPE_P1L0 | MB_TYPE_P1L1,                2, 16,  8},
    /* 13 */{ MB_TYPE_8x16    | MB_TYPE_P0L0 | MB_TYPE_P1L0 | MB_TYPE_P1L1,                2,  8, 16},
    /* 14 */{ MB_TYPE_16x8    | MB_TYPE_P0L1 | MB_TYPE_P1L0 | MB_TYPE_P1L1,                2, 16,  8},
    /* 15 */{ MB_TYPE_8x16    | MB_TYPE_P0L1 | MB_TYPE_P1L0 | MB_TYPE_P1L1,                2,  8, 16},
    /* 16 */{ MB_TYPE_16x8    | MB_TYPE_P0L0 | MB_TYPE_P0L1 | MB_TYPE_P1L0,                2, 16,  8},
    /* 17 */{ MB_TYPE_8x16    | MB_TYPE_P0L0 | MB_TYPE_P0L1 | MB_TYPE_P1L0,                2,  8, 16},
    /* 18 */{ MB_TYPE_16x8    | MB_TYPE_P0L0 | MB_TYPE_P0L1 | MB_TYPE_P1L1,                2, 16,  8},
    /* 19 */{ MB_TYPE_8x16    | MB_TYPE_P0L0 | MB_TYPE_P0L1 | MB_TYPE_P1L1,                2,  8, 16},
    /* 20 */{ MB_TYPE_16x8    | MB_TYPE_P0L0 | MB_TYPE_P0L1 | MB_TYPE_P1L0 | MB_TYPE_P1L1, 2, 16,  8},
    /* 21 */{ MB_TYPE_8x16    | MB_TYPE_P0L0 | MB_TYPE_P0L1 | MB_TYPE_P1L0 | MB_TYPE_P1L1, 2,  8, 16},
    /* 22 */{ MB_TYPE_8x8     | MB_TYPE_P0L0 | MB_TYPE_P0L1 | MB_TYPE_P1L0 | MB_TYPE_P1L1, 4,  8,  8},
};


/* table 7-18 */
const PB_MbInfo b_sub_mb_type_info[13] = {
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
   36, 36, 37, 37, 37, 38, 38, 38, 39, 39,
   39, 39
};
const uint8_t map_4x4[16] = { // this little fucker is bound to be trapped in L1 forever
     0,  1,  4,  5,
     2,  3,  6,  7,
     8,  9, 12, 13,
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



// macroblock neighbors for each 4x4 block in 4x4 grid
int16_t blk_4x4_mb_neighbors[16][4] = {
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


// neighbor 4x4 block index (raster) inside neighbor macroblock
int8_t blk_4x4_neighbor_idx[16][4] = {
    { 3, 12, 13, 15},    { 0, 13, 14, 12},    { 1, 14, 15, 13},    { 2, 15, 12, 14},
    { 7,  0,  1,  3},    { 4,  1, -1,  0},    { 5,  2,  3,  1},    { 6,  3, -1,  2},
    {11,  4,  5,  7},    { 8,  5,  6,  4},    { 9,  6,  7,  5},    {10,  7, -1,  6},
    {15,  8,  9, 11},    {12,  9, -1,  8},    {13, 10, 11,  9},    {14, 11, -1, 10}
};

// neighbor 4x4 block coords inside neighbor macroblock
int8_t blk_4x4_neighbor_coords[16][4][2] = {
    // {y,x}
    { { 0,12},{12, 0},{12, 4},{12,12} },   { { 0, 0},{12, 4},{12, 8},{12, 0} },   { { 0, 4},{12, 8},{12,12},{12, 4} },   { { 0, 8},{12,12},{12, 0},{12, 8} },
    { { 4,12},{ 0, 0},{ 0, 4},{ 0,12} },   { { 4, 0},{ 0, 4},{ 0, 8},{ 0, 0} },   { { 4, 4},{ 0, 8},{ 0,12},{ 0, 4} },   { { 4, 8},{ 0,12},{ 0, 0},{ 0, 8} },
    { { 8,12},{ 4, 0},{ 4, 4},{ 4,12} },   { { 8, 0},{ 4, 4},{ 4, 8},{ 4, 0} },   { { 8, 4},{ 4, 8},{ 4,12},{ 4, 4} },   { { 8, 8},{ 4,12},{ 4, 0},{ 4, 8} },
    { {12,12},{ 8, 0},{ 8, 4},{ 8,12} },   { {12, 0},{ 8, 4},{ 8, 8},{ 8, 0} },   { {12, 4},{ 8, 8},{ 8,12},{ 8, 4} },   { {12, 8},{ 8,12},{ 8, 0},{ 8, 8} },
};


// macroblock neighbors for each 4x4 block in 2x2 grid
int16_t blk_2x2_mb_neighbors[4][4] = {
    {-1, -3, -3, -4},   { 0, -3, -2, -3},
    {-1,  0,  0, -1},   { 0,  0, -5,  0}
};

// neighbor 4x4 block index (raster) inside neighbor macroblock
int8_t blk_2x2_neighbor_idx[4][4] = {
    { 1,  2,  3,  3},   { 0,  3,  2,  2},
    { 3,  0,  1,  1},   { 2,  1, -1,  0},
};

// neighbor 4x4 block coords inside neighbor macroblock
int8_t blk_2x2_neighbor_coords[4][4][2] = {
    { {0,4},{4,0},{4,4},{4,4} },   { {0,0},{4,4},{4,0},{4,0} },
    { {4,4},{0,0},{0,4},{0,4} },   { {4,0},{0,4},{0,0},{0,0} }
};





// time for some well-earned shit now


// first, include the sse version of weighted pred
#define HEIGHT 16
#include "x86_64/weighted_pred_sse4_template.c"
#undef HEIGHT
#define HEIGHT 8
#include "x86_64/weighted_pred_sse4_template.c"
#undef HEIGHT
#define HEIGHT 4
#include "x86_64/weighted_pred_sse4_template.c"
#undef HEIGHT


// then include qpel and inter templates
#define WIDTH 16
#define HEIGHT 16
#include "inter_template.c"        // 16x16
#undef HEIGHT
#define HEIGHT 8
#include "inter_template.c"        // 16x8
#undef WIDTH
#define WIDTH 8
#include "inter_template.c"        // 8x8
#include "inter_chroma_template.c"
#undef HEIGHT
#define HEIGHT 16
#include "inter_template.c"        // 8x16
#undef HEIGHT
#define HEIGHT 4
#include "inter_template.c"        // 8x4
#include "inter_chroma_template.c"
#undef WIDTH
#define WIDTH 4
#include "inter_template.c"        // 4x4
#include "inter_chroma_template.c"
#undef HEIGHT
#define HEIGHT 8
#include "inter_template.c"        // 4x8
#include "inter_chroma_template.c"
#undef HEIGHT
#define HEIGHT 2
#include "inter_chroma_template.c" // 4x2
#undef WIDTH
#define WIDTH 2
#include "inter_chroma_template.c" // 2x2
#undef HEIGHT
#define HEIGHT 4
#include "inter_chroma_template.c" // 2x4

#undef HEIGHT
#undef WIDTH

#define DISPATCH_PART_LUMA(func, w, h, ...) \
    switch (((w) << 8) | (h)) { \
        case (16<<8)|16: func ## _16x16(__VA_ARGS__); break; \
        case (16<<8)|8:  func ## _16x8(__VA_ARGS__);  break; \
        case ( 8<<8)|16: func ## _8x16(__VA_ARGS__);  break; \
        case ( 8<<8)|8:  func ## _8x8(__VA_ARGS__);   break; \
        case ( 8<<8)|4:  func ## _8x4(__VA_ARGS__);   break; \
        case ( 4<<8)|8:  func ## _4x8(__VA_ARGS__);   break; \
        case ( 4<<8)|4:  func ## _4x4(__VA_ARGS__);   break; \
    } \

#define DISPATCH_PART_CHROMA(func, w, h, ...) \
    switch (((w) << 8) | (h)) { \
        case ( 8<<8)|8:  func ## _8x8(__VA_ARGS__);   break; \
        case ( 8<<8)|4:  func ## _8x4(__VA_ARGS__);   break; \
        case ( 4<<8)|8:  func ## _4x8(__VA_ARGS__);   break; \
        case ( 4<<8)|4:  func ## _4x4(__VA_ARGS__);   break; \
        case ( 4<<8)|2:  func ## _4x2(__VA_ARGS__);   break; \
        case ( 2<<8)|4:  func ## _2x4(__VA_ARGS__);   break; \
        case ( 2<<8)|2:  func ## _2x2(__VA_ARGS__);   break; \
    } \


int neighbor_tables_initialized = 0;

void init_neighbor_tables(Undo264Context *ctx) {
    // replace -3 by -mb_width, -2 by -mb_width+1 and -4 by -mb_width-1

    int mb_width = ctx->ps->sps->pic_width_in_mbs;
    for (int i = 0; i < 16; i++) {
        for (int m = 0; m < 4; m++) {
            if      (blk_4x4_mb_neighbors[i][m] == -2) blk_4x4_mb_neighbors[i][m] = -mb_width+1;
            else if (blk_4x4_mb_neighbors[i][m] == -3) blk_4x4_mb_neighbors[i][m] = -mb_width;
            else if (blk_4x4_mb_neighbors[i][m] == -4) blk_4x4_mb_neighbors[i][m] = -mb_width-1;
        }
    }
    for (int i = 0; i < 4; i++) {
        for (int m = 0; m < 4; m++) {
            if      (blk_2x2_mb_neighbors[i][m] == -2) blk_2x2_mb_neighbors[i][m] = -mb_width+1;
            else if (blk_2x2_mb_neighbors[i][m] == -3) blk_2x2_mb_neighbors[i][m] = -mb_width;
            else if (blk_2x2_mb_neighbors[i][m] == -4) blk_2x2_mb_neighbors[i][m] = -mb_width-1;
        }
    }

    neighbor_tables_initialized = 1;
}




void decode_i_macroblock(Macroblock *mb, Slice *slice, Undo264Context *ctx) {
    int chroma_at = slice->sh->sps->chroma_format_idc;

    if (IS_INTRA4x4(mb->mb_type)) {
        for (int i = 0; i < 16; i++) {
            int blkIdx = map_4x4[i];
            int pred_mode = ctx->mb_metadata[mb->mbAddr].intra_NxN_pred_mode[blkIdx];
            intra_pred_4x4(mb, blkIdx, pred_mode, ctx);
            transform_luma_4x4(mb, mb->QPY, blkIdx, ctx);
        }

        if (chroma_at != 0) {
            intra_chroma_pred(mb, ctx);
            transform_chroma(mb, ctx);
        }

    } else if (IS_INTRA16x16(mb->mb_type)) {
        intra_pred_16x16(mb, ctx);
        transform_luma_16x16(mb, mb->QPY, ctx);

        if (chroma_at != 0) {
            intra_chroma_pred(mb, ctx);
            transform_chroma(mb, ctx);
        }

    } else if (IS_INTRA8x8(mb->mb_type)) {
        for (int i8x8 = 0; i8x8 < 4; i8x8++) {
            int pred_mode = ctx->mb_metadata[mb->mbAddr].intra_NxN_pred_mode[i8x8];
            intra_pred_8x8(mb, i8x8, pred_mode, ctx);
            if (mb->residuals.cbp_luma & (1 << i8x8)) {
                transform_luma_8x8(mb, mb->QPY, i8x8, ctx);
            }
        }

        if (chroma_at != 0) {
            intra_chroma_pred(mb, ctx);
            transform_chroma(mb, ctx);
        }
    }


    reset_motion_info(mb->mbAddr, ctx);
}





void decode_p_macroblock(Macroblock *mb, Slice *slice, Undo264Context *ctx) {
    reset_motion_info(mb->mbAddr, ctx);

    int chroma_at = slice->sh->sps->chroma_format_idc;

    if (IS_INTRA(mb->mb_type)) {
        decode_i_macroblock(mb, slice, ctx);
    } else {
        if (IS_SKIP(mb->mb_type)) {
            derive_p_skip_mv(mb, ctx);
        } else {
            if (IS_16x16(mb->mb_type)) {
                derive_16x16_mv(mb, L0, ctx);
            } else if (IS_16x8(mb->mb_type)) {
                derive_16x8_part_mv(mb, 0, L0, ctx);
                derive_16x8_part_mv(mb, 1, L0, ctx);
            } else if (IS_8x16(mb->mb_type)) {
                derive_8x16_part_mv(mb, 0, L0, ctx);
                derive_8x16_part_mv(mb, 1, L0, ctx);
            } else if (IS_8x8(mb->mb_type)) {
                derive_8x8_mv(mb, ctx);
            }
        }

        Picture *pic = mb->p_pic;

        int w = mb->u.pb.mb_info.mb_part_width;
        int h = mb->u.pb.mb_info.mb_part_height;

        if (w == 16 || h == 16) {
            uint8_t *scratch_buf        = ctx->mc_scratch_buffers[(w * h) / 4 - 1];
            uint8_t *scratch_buf_chroma = ctx->mc_scratch_buffers[(w * h) / 8 - 1];
            int16_t *qpel_pass_buf      = ctx->qpel_pass_buffers[w / 4 - 1];

            for (int part = 0; part < mb->u.pb.mb_info.part_count; part++) {
                int pos4x4 = part * ((w == 8) * 2 + (h == 8) * 8);
                MotionVector mv = pic->motion_info[mb->mbAddr][pos4x4].mvs[L0];
                derive_pred_weights(mv.ref_idx, 0, true, false, ctx);

                DISPATCH_PART_LUMA(inter_pred_single, w, h, mb, pos4x4, mv, L0, scratch_buf, qpel_pass_buf, ctx);
                if (chroma_at != 0) {
                    DISPATCH_PART_CHROMA(inter_pred_chroma_single, w / 2, h / 2, mb, pos4x4, mv, L0, scratch_buf_chroma, ctx);
                }
            }
        } else {
            for (int part = 0; part < 4; part++) {
                int subW = mb->u.pb.sub_mb_info[part].mb_part_width;
                int subH = mb->u.pb.sub_mb_info[part].mb_part_height;
                uint8_t *scratch_buf        = ctx->mc_scratch_buffers[(subW * subH) / 4 - 1];
                uint8_t *scratch_buf_chroma = ctx->mc_scratch_buffers[(subW * subH) / 16 - 1];
                int16_t *qpel_pass_buf      = ctx->qpel_pass_buffers[subW / 4 - 1];

                for (int subPart = 0; subPart < mb->u.pb.sub_mb_info[part].part_count; subPart++) {
                    int pos4x4 = map_4x4[part*4] + (subW == 8 && subH == 4) * (subPart * 4) +
                                                   (subW == 4 && subH == 8) * (subPart) +
                                                   (subW == 4 && subH == 4) * (subPart + (subPart/2)*2);
                    MotionVector mv = ctx->curr_pic->motion_info[mb->mbAddr][pos4x4].mvs[L0];
                    derive_pred_weights(mv.ref_idx, 0, true, false, ctx);

                    DISPATCH_PART_LUMA(inter_pred_single, subW, subH, mb, pos4x4, mv, L0, scratch_buf, qpel_pass_buf, ctx);
                    if (chroma_at != 0) {
                        DISPATCH_PART_CHROMA(inter_pred_chroma_single, subW / 2, subH / 2, mb, pos4x4, mv, L0, scratch_buf_chroma, ctx);
                    }
                }
            }
        }


        if (!IS_SKIP(mb->mb_type)) {
            if (mb->t_8x8_flag) {
                for (int i = 0; i < 4; i++) {
                    if ((mb->residuals.cbp_luma >> i) & 1) {
                        transform_luma_8x8(mb, mb->QPY, i, ctx);
                    }
                }
            } else {
                for (int i8x8 = 0; i8x8 < 4; i8x8++) {
                    if ((mb->residuals.cbp_luma >> i8x8) & 1) {
                        for (int i4x4 = 0; i4x4 < 4; i4x4++) {
                            transform_luma_4x4(mb, mb->QPY, map_4x4[i8x8*4+i4x4], ctx);
                        }
                    }
                }
            }
            if (chroma_at != 0)
                transform_chroma(mb, ctx);
        }
    }
}


void decode_b_macroblock(Macroblock *mb,  Slice *slice, Undo264Context *ctx) {
    reset_motion_info(mb->mbAddr, ctx);


    int chroma_at = slice->sh->sps->chroma_format_idc;


    if (IS_INTRA(mb->mb_type)) {
        decode_i_macroblock(mb, slice, ctx);
    } else {
        if (IS_SKIP(mb->mb_type) || IS_DIRECT(mb->mb_type)) {
            for (int part = 0; part < 4; part++) {
                derive_direct_mv(mb, part, ctx);
            }
        } else if (IS_16x16(mb->mb_type)) {
            if (mb->mb_type & MB_TYPE_P0L0) derive_16x16_mv(mb, L0, ctx);
            if (mb->mb_type & MB_TYPE_P0L1) derive_16x16_mv(mb, L1, ctx);
        } else if (IS_16x8(mb->mb_type)) {
            if (mb->mb_type & MB_TYPE_P0L0) derive_16x8_part_mv(mb, 0, L0, ctx);
            if (mb->mb_type & MB_TYPE_P0L1) derive_16x8_part_mv(mb, 0, L1, ctx);
            if (mb->mb_type & MB_TYPE_P1L0) derive_16x8_part_mv(mb, 1, L0, ctx);
            if (mb->mb_type & MB_TYPE_P1L1) derive_16x8_part_mv(mb, 1, L1, ctx);
        } else if (IS_8x16(mb->mb_type)) {
            if (mb->mb_type & MB_TYPE_P0L0) derive_8x16_part_mv(mb, 0, L0, ctx);
            if (mb->mb_type & MB_TYPE_P0L1) derive_8x16_part_mv(mb, 0, L1, ctx);
            if (mb->mb_type & MB_TYPE_P1L0) derive_8x16_part_mv(mb, 1, L0, ctx);
            if (mb->mb_type & MB_TYPE_P1L1) derive_8x16_part_mv(mb, 1, L1, ctx);
        } else if (IS_8x8(mb->mb_type)) {
            derive_8x8_mv(mb, ctx);
        }


        Picture *pic = mb->p_pic;

        if (IS_DIRECT(mb->mb_type) || IS_SKIP(mb->mb_type)) {
            mb->u.pb.mb_info = b_mb_type_info[0];
            bool direct8x8 = slice->sh->sps->direct_8x8_inference_flag;
            PB_MbInfo directInfo = {SUB_MB_TYPE_DIRECT, 4 - direct8x8*3, 4 + direct8x8*4, 4 + direct8x8*4};
            for (int i = 0; i < 4; i++ ) {
                mb->u.pb.sub_mb_info[i] = directInfo;
            }
        }

        int w = mb->u.pb.mb_info.mb_part_width;
        int h = mb->u.pb.mb_info.mb_part_height;



        if (w == 16 || h == 16) {
            uint8_t *scratch_buf        = ctx->mc_scratch_buffers[(w * h) / 4 - 1];
            uint8_t *scratch_buf_chroma = ctx->mc_scratch_buffers[(w * h) / 8 - 1];
            int16_t *qpel_pass_buf      = ctx->qpel_pass_buffers[w / 4 - 1];

            uint8_t *temp_bi_buf = ctx->mc_temp_bi_buffers[(w * h) / 4 - 1];
            uint8_t *temp_bi_buf_chroma = ctx->mc_temp_bi_buffers[(w * h) / 8 - 1];

            for (int part = 0; part < mb->u.pb.mb_info.part_count; part++) {
                int pos4x4 = part * ((w == 8) * 2 + (h == 8) * 8);
                int pos2x2 = map_4x4[pos4x4] / 4;
                bool l0 = ctx->curr_pic->pred_flags[mb->mbAddr][L0][pos2x2];
                bool l1 = ctx->curr_pic->pred_flags[mb->mbAddr][L1][pos2x2];

                if (l0 + l1 == 1) {
                    int list = l0 ? L0 : L1;

                    MotionVector mv = pic->motion_info[mb->mbAddr][pos4x4].mvs[list];
                    derive_pred_weights(mv.ref_idx, mv.ref_idx, l0, l1, ctx);


                    DISPATCH_PART_LUMA(inter_pred_single, w, h, mb, pos4x4, mv, list, scratch_buf, qpel_pass_buf, ctx);
                    if (chroma_at != 0) {
                        DISPATCH_PART_CHROMA(inter_pred_chroma_single, w / 2, h / 2, mb, pos4x4, mv, list, scratch_buf_chroma, ctx);
                    }
                } else if (l0 + l1 == 2) {
                    MotionVector mvL0 = ctx->curr_pic->motion_info[mb->mbAddr][pos4x4].mvs[L0];
                    MotionVector mvL1 = ctx->curr_pic->motion_info[mb->mbAddr][pos4x4].mvs[L1];

                    derive_pred_weights(mvL0.ref_idx, mvL1.ref_idx, true, true, ctx);

                    DISPATCH_PART_LUMA(inter_pred_bi, w, h, mb, pos4x4, mvL0, mvL1, scratch_buf, temp_bi_buf, qpel_pass_buf, ctx);
                    if (chroma_at != 0) {
                        DISPATCH_PART_CHROMA(inter_pred_chroma_bi, w / 2, h / 2, mb, pos4x4, mvL0, mvL1, scratch_buf_chroma, temp_bi_buf_chroma, ctx);
                    }
                }
            }
        } else {
            for (int part = 0; part < 4; part++) {
                int subW = mb->u.pb.sub_mb_info[part].mb_part_width;
                int subH = mb->u.pb.sub_mb_info[part].mb_part_height;

                uint8_t *scratch_buf        = ctx->mc_scratch_buffers[(subW * subH) / 4 - 1];
                uint8_t *scratch_buf_chroma = ctx->mc_scratch_buffers[(subW * subH) / 8 - 1];
                int16_t *qpel_pass_buf      = ctx->qpel_pass_buffers[subW / 4 - 1];

                uint8_t *temp_bi_buf = ctx->mc_temp_bi_buffers[(subW * subH) / 4 - 1];
                uint8_t *temp_bi_buf_chroma = ctx->mc_temp_bi_buffers[(subW * subH) / 8 - 1];

                bool l0 = ctx->curr_pic->pred_flags[mb->mbAddr][L0][part];
                bool l1 = ctx->curr_pic->pred_flags[mb->mbAddr][L1][part];

                if (l0 + l1 == 1) {
                    int list = l0 ? L0 : L1;
                    for (int subPart = 0; subPart < mb->u.pb.sub_mb_info[part].part_count; subPart++) {
                        int pos4x4 = map_4x4[part*4] + (subW == 8 && subH == 4) * (subPart * 4) +
                                                   (subW == 4 && subH == 8) * (subPart) +
                                                   (subW == 4 && subH == 4) * (subPart + (subPart/2)*2);
                        MotionVector mv = ctx->curr_pic->motion_info[mb->mbAddr][pos4x4].mvs[list];
                        derive_pred_weights(mv.ref_idx, mv.ref_idx, l0, l1, ctx);

                        DISPATCH_PART_LUMA(inter_pred_single, subW, subH, mb, pos4x4, mv, list, scratch_buf, qpel_pass_buf, ctx);
                        if (chroma_at != 0) {
                            DISPATCH_PART_CHROMA(inter_pred_chroma_single, subW / 2, subH / 2, mb, pos4x4, mv, list, scratch_buf_chroma, ctx);
                        }
                    }
                } else if (l0 + l1 == 2) {
                    for (int subPart = 0; subPart < mb->u.pb.sub_mb_info[part].part_count; subPart++) {
                        int pos4x4 = map_4x4[part*4] + (subW == 8 && subH == 4) * (subPart * 4) +
                                                   (subW == 4 && subH == 8) * (subPart) +
                                                   (subW == 4 && subH == 4) * (subPart + (subPart/2)*2);
                        MotionVector mvL0 = ctx->curr_pic->motion_info[mb->mbAddr][pos4x4].mvs[L0];
                        MotionVector mvL1 = ctx->curr_pic->motion_info[mb->mbAddr][pos4x4].mvs[L1];
                        derive_pred_weights(mvL0.ref_idx, mvL1.ref_idx, true, true, ctx);

                        DISPATCH_PART_LUMA(inter_pred_bi, subW, subH, mb, pos4x4, mvL0, mvL1, scratch_buf, temp_bi_buf, qpel_pass_buf, ctx);
                        if (chroma_at != 0) {
                            DISPATCH_PART_CHROMA(inter_pred_chroma_bi, subW / 2, subH / 2, mb, pos4x4, mvL0, mvL1, scratch_buf_chroma, temp_bi_buf_chroma, ctx);
                        }
                    }
                }
            }
        }




        if (!IS_SKIP(mb->mb_type)) {
            if (mb->t_8x8_flag) {
                for (int i = 0; i < 4; i++) {
                    transform_luma_8x8(mb, mb->QPY, i, ctx);
                }
            } else {
                for (int i = 0; i < 16; i++) {
                    transform_luma_4x4(mb, mb->QPY, map_4x4[i], ctx);
                }
            }
            if (chroma_at != 0)
                transform_chroma(mb, ctx);
        }
    }
}