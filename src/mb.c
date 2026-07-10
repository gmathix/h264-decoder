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
    /*  0 */{ MB_TYPE_DIRECT | MB_TYPE_L0L1,                                              1,  8,  8},
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







int neighbor_tables_initialized = 0;

void init_neighbor_tables(CodecContext *ctx) {
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



/* 7.3.5 */
void read_macroblock(Macroblock *mb, SliceHeader *sh, NalUnit *nal_unit, CodecContext *ctx) {
    BitReader *br = ctx->br;


    PPS *pps = sh->pps;
    SPS *sps = sh->sps;


    uint32_t mb_type = read_ue(br);
    mb->table_idx = mb_type;


    if ((IS_I_SLICE(sh->slice_type) && mb_type > 25) ||
        (IS_P_SLICE(sh->slice_type) && mb_type > 30)  ||
        (IS_B_SLICE(sh->slice_type) && mb_type > 48)) {

        printf("mb_type out of bounds for %s slice : got %d (mbAddr:%d)\n",
            slice_type_to_string(sh->slice_type), mb_type, mb->mbAddr);
        return;
    }

    mb->mb_height_c = 16 / sub_height_c_info[sh->sps->chroma_format_idc];
    mb->mb_width_c  = 16 / sub_width_c_info[sh->sps->chroma_format_idc];

    mb->QPY = mb->mbAddr == 0
                ? sh->pps->pic_init_qp + sh->slice_qp_delta
                : ctx->prevMb->QPY;


    uint8_t pcm_samples_luma[256];
    uint8_t pcm_samples_chroma[2 * mb->mb_height_c * mb->mb_width_c];

    bool intra_mb = IS_I_SLICE(sh->slice_type) || (IS_P_SLICE(sh->slice_type) && mb_type > 4) || (IS_B_SLICE(sh->slice_type) && mb_type > 22);
    if (intra_mb && IS_P_SLICE(sh->slice_type)) {
        mb_type -= 5;
        mb->table_idx -= 5;
    } else if (intra_mb && IS_B_SLICE(sh->slice_type)) {
        mb_type -= 23;
        mb->table_idx -= 23;
    }

    if (intra_mb) {
        mb->u.i.mb_info = i_mb_type_info[mb_type];
        mb->mb_type     = i_mb_type_info[mb_type].type;
    } else if (IS_P_SLICE(sh->slice_type)) {
        mb->u.pb.mb_info = p_mb_type_info[mb_type];
        mb->mb_type      = p_mb_type_info[mb_type].type;
    } else if (IS_B_SLICE(sh->slice_type)) {
        mb->u.pb.mb_info = b_mb_type_info[mb_type];
        mb->mb_type      = b_mb_type_info[mb_type].type;
    }
    // if (ctx->prf->total_frames == 16) {
    //     printf("mb %d : type %d (%s)\n", mb->mbAddr, mb_type, mb_type_to_string(mb->mb_type));
    // }


    ctx->curr_pic->mb_types[mb->mbAddr] = mb->mb_type;
    int type = mb->mb_type;

    if (intra_mb) mb->pred_mode = i_mb_type_info[mb_type].pred_mode;
    else mb->pred_mode = -1;

    mb->slice_type = sh->slice_type;

    int pred_mode = intra_mb
        ? i_mb_type_info[mb_type].pred_mode
        : -1;

    int cbp_luma = intra_mb
        ? i_mb_type_info[mb_type].cbp_luma
        : -1;

    int cbp_chroma = intra_mb
        ? i_mb_type_info[mb_type].cbp_chroma
        : -1;

    mb->residuals.cbp_chroma = cbp_chroma;
    mb->residuals.cbp_luma = cbp_luma;


    memset(ctx->luma_total_coeffs[mb->mbAddr], 0, 16);
    memset(ctx->cb_total_coeffs[mb->mbAddr], 0, 16);
    memset(ctx->cr_total_coeffs[mb->mbAddr], 0, 16);

    MacroblockMetadata *meta = &ctx->mb_metadata[mb->mbAddr];
    meta->mb_type    = mb->mb_type;
    meta->cbp_luma   = cbp_luma;
    meta->cbp_chroma = cbp_chroma;
    meta->t_8x8_flag = 0;





    if (type == MB_TYPE_INTRA_PCM) { // just inject the samples directly
        while (!bitreader_byte_aligned(br)) {
            bitreader_skip_bits(br, 1);
        }

        int widthY = mb->p_pic->widthY;
        int widthC = mb->p_pic->widthC;
        int posY = mb->mb_y*16*widthY+ mb->mb_x*16;
        int posC = mb->mb_y*mb->mb_height_c*widthC + mb->mb_x*mb->mb_width_c;

        for (int i = 0; i < 256; i++) {
            mb->p_pic->luma[posY + (i/16)*widthY + i%16] = read_u(br, 8);
        }
        for (int i = 0; i < mb->mb_width_c * mb->mb_height_c; i++) {
            mb->p_pic->cb[posC + (i/mb->mb_height_c)*widthC + i%mb->mb_width_c] = read_u(br, 8);
        }
        for (int i = 0; i < mb->mb_width_c * mb->mb_height_c; i++) {
            mb->p_pic->cr[posC + (i/mb->mb_height_c)*widthC + i%mb->mb_width_c] = read_u(br, 8);
        }
    } else {

        bool noSubMbPartSizeLessThan8x8 = true;

        if (!intra_mb && mb->u.pb.mb_info.part_count == 4) {
            read_sub_mb_pred(mb, sh, ctx);
            for (int part = 0; part < 4; part++) {
                if (!IS_SUB_DIRECT(mb->u.pb.sub_mb_info[part].type)) {
                    if (mb->u.pb.sub_mb_info[part].part_count > 1) {
                        noSubMbPartSizeLessThan8x8 = false;
                    }
                } else if (!sps->direct_8x8_inference_flag) {
                    noSubMbPartSizeLessThan8x8 = false;
                }
            }
        } else {
            if (pps->transform_8x8_mode_flag && IS_INTRA4x4(type)) {
                mb->t_8x8_flag = read_u(br, 1);
                meta->t_8x8_flag = mb->t_8x8_flag;
                if (mb->t_8x8_flag) {
                    mb->mb_type = MB_TYPE_INTRA8x8;
                    meta->mb_type = mb->mb_type;
                    ctx->curr_pic->mb_types[mb->mbAddr]  = mb->mb_type;
                }
            }
            read_mb_pred(mb, sh, ctx);
        }

        if (!IS_INTRA16x16(type)) {
            int32_t cbp = map_coded_block_pattern(read_ue(br), sps->chroma_format_idc,
                intra_mb);

            cbp_luma   = cbp % 16;
            cbp_chroma = cbp / 16;
            mb->residuals.cbp_chroma = cbp_chroma;
            mb->residuals.cbp_luma = cbp_luma;
            meta->cbp_luma = cbp_luma;
            meta->cbp_chroma = cbp_chroma;

            if (cbp_luma > 0 && pps->transform_8x8_mode_flag &&
                !IS_INTRA4x4(type) && noSubMbPartSizeLessThan8x8 &&
                (!IS_DIRECT(type) || sps->direct_8x8_inference_flag)) {

                mb->t_8x8_flag = read_u(br, 1);
                meta->t_8x8_flag = mb->t_8x8_flag;
            }
        }

        if (cbp_luma > 0 || cbp_chroma > 0 || IS_INTRA16x16(type)) {

            mb->mb_qp_delta = read_se(br);

            mb->QPY = mb->mbAddr == 0
                ? _clip3(0, 51, (pps->pic_init_qp + sh->slice_qp_delta + mb->mb_qp_delta + 52) % 52)
                : _clip3(0, 51, (ctx->prevMb->QPY + mb->mb_qp_delta + 52) % 52);


            int qPi = _clip3(0, 51, mb->QPY + pps->chroma_qp_index_offset);
            mb->QPC = QPcTable[qPi];


            meta->QPY = mb->QPY;
            meta->QPC = mb->QPC;



            residual_func residual_block = sh->pps->cabac_flag
                ? &residual_block_cabac
                : &residual_block_cavlc;


            read_residual(mb, type, mb->t_8x8_flag, 0, 15, cbp_luma, cbp_chroma,
                        residual_block, sh, ctx);
        } else {
            mb->QPY = mb->mbAddr == 0
                ? _clip3(0, 51, (pps->pic_init_qp + sh->slice_qp_delta + 52) % 52)
                : ctx->prevMb->QPY;

            int qPi = _clip3(0, 51, mb->QPY + pps->chroma_qp_index_offset);
            mb->QPC = QPcTable[qPi];

            meta->QPY = mb->QPY;
            meta->QPC = mb->QPC;
        }
    }


    ctx->prevMb = mb;
}


/* 7.3.5.1 */
void read_mb_pred(Macroblock *mb, SliceHeader *sh, CodecContext *ctx) {
    BitReader *br = ctx->br;
    Picture *currPic = ctx->curr_pic;

    int mbAddr = mb->mbAddr;

    if (IS_INTRA4x4(mb->mb_type) ||
        IS_INTRA8x8(mb->mb_type) ||
        IS_INTRA16x16(mb->mb_type)) {

        if (IS_INTRA4x4(mb->mb_type)) {
            int intraModeA, intraModeB;
            for (int i = 0; i < 16; i++) {
                int blkIdx = map_4x4[i];
                Neighbors n = derive_neighbors_4x4(mb, blkIdx, ctx);
                int dcPredModePredictedFlag;


                int mb_a_type = n.a.av
                    ? ctx->mb_metadata[mbAddr + n.a.mb_off].mb_type
                    : mb->mb_type;
                int mb_b_type = n.b.av
                    ? ctx->mb_metadata[mbAddr + n.b.mb_off].mb_type
                    : mb->mb_type;

                dcPredModePredictedFlag = (!n.a.av || !n.b.av ||
                    (n.a.av && IS_INTER(mb_a_type) && ctx->ps->pps->constrained_intra_pred_flag) ||
                    (n.b.av && IS_INTER(mb_b_type) && ctx->ps->pps->constrained_intra_pred_flag));

                if (dcPredModePredictedFlag || !IS_INTRANxN(mb_a_type)) {
                    intraModeA = DC_PRED;
                } else {
                    intraModeA = IS_INTRA4x4(mb_a_type)
                        ? ctx->intra4x4_pred_modes[mbAddr + n.a.mb_off][n.a.idx]
                        : ctx->intra8x8_pred_modes[mbAddr + n.a.mb_off][map_4x4[n.a.idx] / 4];
                }
                if (dcPredModePredictedFlag || !IS_INTRANxN(mb_b_type)) {
                    intraModeB = DC_PRED;
                } else {
                    intraModeB = IS_INTRA4x4(mb_b_type)
                        ? ctx->intra4x4_pred_modes[mbAddr + n.b.mb_off][n.b.idx]
                        : ctx->intra8x8_pred_modes[mbAddr + n.b.mb_off][map_4x4[n.b.idx] / 4];
                }

                int predIntraMode = _min(intraModeA, intraModeB);

                uint8_t prev_intra4x4_pred_mode_flag = read_u(br, 1);
                if (!prev_intra4x4_pred_mode_flag) {
                    uint8_t rem_intra4x4_pred_mode = read_u(br, 3);
                    ctx->intra4x4_pred_modes[mbAddr][blkIdx] = rem_intra4x4_pred_mode < predIntraMode
                        ? rem_intra4x4_pred_mode
                        : rem_intra4x4_pred_mode+1;
                } else {
                    ctx->intra4x4_pred_modes[mbAddr][blkIdx] = predIntraMode;
                }
            }
        }
        else if (IS_INTRA8x8(mb->mb_type)) {
            for (int i8x8 = 0; i8x8 < 4; i8x8++) {
                Neighbors n = derive_neighbors_2x2(mb, i8x8, ctx);

                int aType = currPic->mb_types[mb->mbAddr + n.a.mb_off];
                int bType = currPic->mb_types[mb->mbAddr + n.b.mb_off];

                bool dcModePredicted = (!n.a.av || !n.b.av ||
                    (n.a.av && IS_INTER(currPic->mb_types[mb->mbAddr + n.a.mb_off]) && sh->pps->constrained_intra_pred_flag) ||
                    (n.b.av && IS_INTER(currPic->mb_types[mb->mbAddr + n.b.mb_off]) && sh->pps->constrained_intra_pred_flag));

                int intraModeA, intraModeB;

                if (dcModePredicted || !IS_INTRANxN(aType)) {
                    intraModeA = DC_PRED;
                } else {
                    intraModeA = IS_INTRA8x8(aType)
                        ? ctx->intra8x8_pred_modes[mb->mbAddr + n.a.mb_off][n.a.idx]
                        : ctx->intra4x4_pred_modes[mb->mbAddr + n.a.mb_off][map_4x4[n.a.idx * 4 + 1]];
                }
                if (dcModePredicted || !IS_INTRANxN(bType)) {
                    intraModeB = DC_PRED;
                } else {
                    intraModeB = IS_INTRA8x8(bType)
                        ? ctx->intra8x8_pred_modes[mb->mbAddr + n.b.mb_off][n.b.idx]
                        : ctx->intra4x4_pred_modes[mb->mbAddr + n.b.mb_off][map_4x4[n.b.idx * 4 + 2]];
                }

                int predMode = _min(intraModeA, intraModeB);

                uint8_t prev_intra8x8_pred_mode_flag = read_u(br, 1);
                if (!prev_intra8x8_pred_mode_flag) {
                    uint8_t rem_intra8x8_pred_mode = read_u(br, 3);
                    ctx->intra8x8_pred_modes[mb->mbAddr][i8x8] = rem_intra8x8_pred_mode < predMode
                        ? rem_intra8x8_pred_mode
                        : rem_intra8x8_pred_mode + 1;
                } else {
                    ctx->intra8x8_pred_modes[mb->mbAddr][i8x8] = predMode;
                }
            }
        }
        if (sh->sps->chroma_format_idc == 1 || sh->sps->chroma_format_idc == 2) {
            mb->intra_chroma_pred_mode = read_ue(br);
        }
    } else if (!IS_DIRECT(mb->mb_type)) {
        int type = mb->u.pb.mb_info.type;

        for (int part = 0; part < mb->u.pb.mb_info.part_count; part++) {
            if (sh->num_ref_idx_l0_active_minus1 > 0 &&
                ((part == 0 && (type & MB_TYPE_P0L0)) ||
                 (part == 1 && (type & MB_TYPE_P1L0)))) {
                mb->u.pb.ref_idx[L0][part] = read_te(br, sh->num_ref_idx_l0_active_minus1);
            }
        }
        for (int part = 0; part < mb->u.pb.mb_info.part_count; part++) {
            if (sh->num_ref_idx_l1_active_minus1 > 0 &&
                ((part == 0 && (type & MB_TYPE_P0L1)) ||
                 (part == 1 && (type & MB_TYPE_P1L1)))) {
                mb->u.pb.ref_idx[L1][part] = read_te(br, sh->num_ref_idx_l1_active_minus1);
            }
        }
        for (int part = 0; part < mb->u.pb.mb_info.part_count; part++) {
            if ((part == 0 && (type & MB_TYPE_P0L0)) ||
                (part == 1 && (type & MB_TYPE_P1L0))) {
                for (int i = 0; i < 2; i++) {
                    mb->u.pb.mvd[L0][part][0][i] = read_se(br);
                }
            }
        }
        for (int part = 0; part < mb->u.pb.mb_info.part_count; part++) {
            if ((part == 0 && (type & MB_TYPE_P0L1)) ||
                (part == 1 && (type & MB_TYPE_P1L1))) {
                for (int i = 0; i < 2; i++) {
                    mb->u.pb.mvd[L1][part][0][i] = read_se(br);
                }
            }
        }
    }
}


void read_sub_mb_pred(Macroblock *mb, SliceHeader *sh, CodecContext *ctx) {
    BitReader *br = ctx->br;

    for (int part = 0; part < 4; part++) {
        uint32_t sub_mb_type = read_ue(br);
        mb->u.pb.sub_mb_info[part] = IS_P_SLICE(sh->slice_type)
            ? p_sub_mb_type_info[sub_mb_type]
            : b_sub_mb_type_info[sub_mb_type];
    }
    for (int part = 0; part < 4; part++) {
        if (sh->num_ref_idx_l0_active_minus1 > 0 && !(mb->mb_type & MB_TYPE_REF0) &&
            !(mb->u.pb.sub_mb_info[part].type & SUB_MB_TYPE_DIRECT && IS_B_SLICE(sh->slice_type)) &&
            (mb->u.pb.sub_mb_info[part].type & MB_TYPE_P0L0)) {
            mb->u.pb.ref_idx[L0][part] = read_te(br, sh->num_ref_idx_l0_active_minus1);
        }
    }
    for (int part = 0; part < 4; part++) {
        if (sh->num_ref_idx_l1_active_minus1 > 0 && !(mb->mb_type & MB_TYPE_REF0) &&
            !(mb->u.pb.sub_mb_info[part].type & SUB_MB_TYPE_DIRECT && IS_B_SLICE(sh->slice_type)) &&
            (mb->u.pb.sub_mb_info[part].type & MB_TYPE_P0L1)) {
            mb->u.pb.ref_idx[L1][part] = read_te(br, sh->num_ref_idx_l1_active_minus1);
        }
    }
    for (int part = 0; part < 4; part++) {
        if (!(mb->u.pb.sub_mb_info[part].type & SUB_MB_TYPE_DIRECT && IS_B_SLICE(sh->slice_type)) &&
            (mb->u.pb.sub_mb_info[part].type & MB_TYPE_P0L0)) {
            for (int subPart = 0; subPart < mb->u.pb.sub_mb_info[part].part_count; subPart++) {
                for (int i = 0; i < 2; i++) {
                    mb->u.pb.mvd[L0][part][subPart][i] = read_se(br);
                }
            }
        }
    }
    for (int part = 0; part < 4; part++) {
        if (!(mb->u.pb.sub_mb_info[part].type & SUB_MB_TYPE_DIRECT && IS_B_SLICE(sh->slice_type)) &&
            (mb->u.pb.sub_mb_info[part].type & MB_TYPE_P0L1)) {
            for (int subPart = 0; subPart < mb->u.pb.sub_mb_info[part].part_count; subPart++) {
                for (int i = 0; i < 2; i++) {
                    mb->u.pb.mvd[L1][part][subPart][i] = read_se(br);
                }
            }
        }
    }
}


/* 7.3.5.3 */

void read_residual(Macroblock *mb, int type, int t_8x8_flag, int startIdx, int endIdx, int cbp_luma, int cbp_chroma, residual_func residual_block, SliceHeader *sh, CodecContext *ctx) {
    BitReader *br = ctx->br;



    read_residual_luma(mb, type, t_8x8_flag, cbp_luma, startIdx, endIdx,
                    residual_block, sh, ctx);



    if (sh->sps->chroma_format_idc == 1 || sh->sps->chroma_format_idc == 2) {

        int numC8x8 = 4 /
            (sub_width_c_info[sh->sps->chroma_format_idc] * sub_height_c_info[sh->sps->chroma_format_idc]);


        for (int iCbCr = 0; iCbCr < 2; iCbCr++) {
            uint8_t (*chroma_coeff_table)[16] = iCbCr
                ? ctx->cr_total_coeffs
                : ctx->cb_total_coeffs;

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
                ? ctx->cr_total_coeffs
                : ctx->cb_total_coeffs;

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
        read_residual_luma(mb, type, t_8x8_flag, cbp_chroma, startIdx,  endIdx,
                         residual_block, sh, ctx);

        int16_t CrIntra16x16DC[16];
        int16_t CrIntra16x16AC[16][15];
        int16_t Cr4x4[16][16];
        int16_t Cr8x8[4][64];
        read_residual_luma(mb, type, t_8x8_flag, cbp_chroma, startIdx,  endIdx,
                        residual_block, sh, ctx);
    }
}



/* 7.3.5.3.1 */
void read_residual_luma(Macroblock *mb, int type, int t_8x8_flag, int cbp_luma,
                    int startIdx, int endIdx,
                    residual_func residual_block, SliceHeader *sh, CodecContext *ctx) {

    BitReader *br = ctx->br;


    if (startIdx == 0 && IS_INTRA16x16(type)) {
        (*residual_block)(mb, 0, 0, LUMA_INTRA_16x16_DC_LEVEL, mb->residuals.luma_16x16_DC, ctx->luma_total_coeffs, 0, 15, 16, true, sh, ctx);
    }

    for (int i8x8 = 0; i8x8 < 4; i8x8++) {
        if (!t_8x8_flag || !sh->pps->cabac_flag) {
            for (int i4x4 = 0; i4x4 < 4; i4x4++) {
                int blkIdx = map_4x4[i8x8*4+i4x4];
                if (cbp_luma & (1 << i8x8)) {
                    if (IS_INTRA16x16(type)) {
                        (*residual_block)(mb, blkIdx, 0, LUMA_INTRA_16x16_AC_LEVEL, mb->residuals.luma_16x16_AC[blkIdx], ctx->luma_total_coeffs,
                            _max(0, startIdx - 1), endIdx - 1, 15, true, sh, ctx);
                    } else {
                        (*residual_block)(mb, blkIdx, 0, LUMA_LEVEL_4x4, mb->residuals.luma_4x4_coeffs[blkIdx], ctx->luma_total_coeffs,
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

                if (!sh->pps->cabac_flag && t_8x8_flag) {
                    for (int i = 0; i < 16; i++) {
                        mb->residuals.luma_8x8_coeffs[i8x8][4*i + i4x4] = mb->residuals.luma_4x4_coeffs[blkIdx][i];
                    }
                }
            }
        } else if (cbp_luma & (1 << i8x8)) {
            (*residual_block)(mb, 0, 0, LUMA_LEVEL_8x8, mb->residuals.luma_8x8_coeffs[i8x8], ctx->luma_total_coeffs,
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
    if (mb->mbAddr == mb_debug && ctx->prf->total_frames == frame_debug) {
        debugging = true;
    } else {
        debugging = false;
    }

    if (debugging) {
        fprintf(stderr, "DEBUGGING MB %d type:%s qpy:%d qpc:%d\n",
            mb->mbAddr, mb_type_to_string(mb->mb_type), mb->QPY, mb->QPC);

    }


    if (IS_INTRA4x4(mb->mb_type)) {
        for (int i = 0; i < 16; i++) {
            int blkIdx = map_4x4[i];
            int pred_mode = ctx->intra4x4_pred_modes[mb->mbAddr][blkIdx];
            intra_pred_4x4(mb, blkIdx, pred_mode, ctx);
            transform_luma_4x4(mb, mb->QPY, blkIdx, ctx);
        }

        intra_chroma_pred(mb, ctx);
        transform_chroma(mb, ctx);

    } else if (IS_INTRA16x16(mb->mb_type)) {
        intra_pred_16x16(mb, ctx);
        transform_luma_16x16(mb, mb->QPY, ctx);

        intra_chroma_pred(mb, ctx);
        transform_chroma(mb, ctx);

    } else if (IS_INTRA8x8(mb->mb_type)) {
        for (int i8x8 = 0; i8x8 < 4; i8x8++) {
            int pred_mode = ctx->intra8x8_pred_modes[mb->mbAddr][i8x8];
            intra_pred_8x8(mb, i8x8, pred_mode, ctx);
            if (mb->residuals.cbp_luma & (1 << i8x8)) {
                transform_luma_8x8(mb, mb->QPY, i8x8, ctx);
            }
        }

        if (debugging) {
            for (int y = 0; y < 16; y++) {
                for (int x = 0; x < 16; x++) {
                    printf("%3d ", mb->p_pic->luma[(mb->mb_y*16 + y)*mb->p_pic->widthY + (mb->mb_x*16) + x]);
                }
                printf("\n");
            }
            printf("\n");
        }

        intra_chroma_pred(mb, ctx);
        transform_chroma(mb, ctx);
    }



    for (int i = 0; i < 16; i++) {
        ctx->curr_pic->motion_info[mb->mbAddr][i].mvs[L0] = (MotionVector) {-1, 0, 0};
        ctx->curr_pic->motion_info[mb->mbAddr][i].mvs[L1] = (MotionVector) {-1, 0, 0};
    }
    memset(&ctx->curr_pic->pred_flags[mb->mbAddr][L0][0], false, 4);
    memset(&ctx->curr_pic->pred_flags[mb->mbAddr][L1][0], false, 4);
}





void decode_p_macroblock(Macroblock *mb, Slice *slice, CodecContext *ctx) {

    if (mb->mbAddr == mb_debug && ctx->prf->total_frames == frame_debug) {
        debugging = true;
    } else {
        debugging = false;
    }

    for (int i = 0; i < 16; i++) {
        ctx->curr_pic->motion_info[mb->mbAddr][i].mvs[L0] = (MotionVector) {-1, 0, 0};
        ctx->curr_pic->motion_info[mb->mbAddr][i].mvs[L1] = (MotionVector) {-1, 0, 0};
    }
    memset(&ctx->curr_pic->pred_flags[mb->mbAddr][L0][0], false, 4);
    memset(&ctx->curr_pic->pred_flags[mb->mbAddr][L1][0], false, 4);

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



        for (int part = 0; part < 4; part++) {
            for (int subPart = 0; subPart < 4; subPart++) {
                int idx = map_4x4[part * 4 + subPart];
                MotionVector *mv = &ctx->curr_pic->motion_info[mb->mbAddr][idx].mvs[L0];
                derive_pred_weights(mv->ref_idx, 0, true, false, ctx);

                inter_pred_single(mb, idx, mv, L0, ctx);
                inter_pred_chroma_single(mb, idx, mv, L0, ctx);
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
            transform_chroma(mb, ctx);
        }
    }
}


void decode_b_macroblock(Macroblock *mb,  Slice *slice, CodecContext *ctx) {

    if (mb->mbAddr == mb_debug && ctx->curr_pic->poc == poc_debug && ctx->curr_pic->frame_num == frame_num_debug) {
        debugging = true;
    } else {
        debugging = false;
    }

    if (debugging) {
        fprintf(stderr, "DEBUGGING MB %d type:%s qpy:%d qpc:%d\n",
            mb->mbAddr, mb_type_to_string(mb->mb_type), mb->QPY, mb->QPC);

    }

    // printf("%d\n", mb->mbAddr);


    for (int i = 0; i < 16; i++) {
        ctx->curr_pic->motion_info[mb->mbAddr][i].mvs[L0] = (MotionVector) {-1, 0, 0};
        ctx->curr_pic->motion_info[mb->mbAddr][i].mvs[L1] = (MotionVector) {-1, 0, 0};
    }
    memset(&ctx->curr_pic->pred_flags[mb->mbAddr][L0][0], false, 4);
    memset(&ctx->curr_pic->pred_flags[mb->mbAddr][L1][0], false, 4);

    if (IS_INTRA(mb->mb_type)) {
        decode_i_macroblock(mb, slice, ctx);
    } else {
        if (IS_SKIP(mb->mb_type) || IS_DIRECT(mb->mb_type)) {
            for (int part = 0; part < 4; part++) derive_direct_mv(mb, part, ctx);
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


        /* luma inter pred */
        for (int part = 0; part < 4; part++) {
            bool l0 = ctx->curr_pic->pred_flags[mb->mbAddr][L0][part];
            bool l1 = ctx->curr_pic->pred_flags[mb->mbAddr][L1][part];


            for (int subPart = 0; subPart < 4; subPart++) {
                int idx = map_4x4[part * 4 + subPart];

                if (l0 + l1 == 1) {
                    int list = l0 == 1 ? L0 : L1;
                    MotionVector *mv = &ctx->curr_pic->motion_info[mb->mbAddr][idx].mvs[list];
                    if (debugging) fprintf(stderr, "part %d sub %d --- %s mv:(%d,%d)\n", part, subPart, l0 ? "L0" : "L1", mv->x, mv->y);

                    derive_pred_weights(mv->ref_idx, mv->ref_idx, l0, l1, ctx);

                    inter_pred_single(mb, idx, mv, list, ctx);
                    inter_pred_chroma_single(mb, idx,  mv, list, ctx);
                } else if (l0 + l1 == 2) {
                    MotionVector *mvL0 = &ctx->curr_pic->motion_info[mb->mbAddr][idx].mvs[L0];
                    MotionVector *mvL1 = &ctx->curr_pic->motion_info[mb->mbAddr][idx].mvs[L1];
                    if (debugging) fprintf(stderr, "part %d sub %d --- L0 L1 mv0:(%d,%d) mv1:(%d,%d)\n", part, subPart,  mvL0->x, mvL0->y, mvL1->x, mvL1->y);

                    derive_pred_weights(mvL0->ref_idx, mvL1->ref_idx, true, true, ctx);

                    inter_pred_bi(mb, idx, mvL0, mvL1, ctx);
                    inter_pred_chroma_bi(mb, idx, mvL0, mvL1, ctx);
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
            transform_chroma(mb, ctx);
        }
    }
}