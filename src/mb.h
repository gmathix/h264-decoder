//
// Created by gmathix on 3/21/26.
//

#ifndef TOY_H264_MB_H
#define TOY_H264_MB_H



#include "global.h"
#include "slice.h"

#include "util/formulas.h"


typedef struct I_MbInfo {
    int type;
    int pred_mode;
    int cbp_chroma;
    int cbp_luma;
} I_MbInfo ;
extern const I_MbInfo i_mb_type_info[26];


typedef struct PB_MbInfo {
    int type;
    uint8_t  part_count;
    uint8_t  mb_part_width;
    uint8_t  mb_part_height;
} PB_MbInfo ;
extern const PB_MbInfo p_mb_type_info[5];
extern const PB_MbInfo p_sub_mb_type_info[4];
extern const PB_MbInfo b_mb_type_info[23];
extern const PB_MbInfo b_sub_mb_type_info[13];


extern const int sub_width_c_info[4];
extern const int sub_height_c_info[4];


extern const int QPcTable[52];
extern const int luma_location_diff[4][2];
extern const uint8_t map_4x4[16];


extern int16_t blk_4x4_mb_neighbors        [16][4];
extern int8_t  blk_4x4_neighbor_idx        [16][4];
extern int8_t  blk_4x4_neighbor_coords  [16][4][2];

extern int16_t blk_2x2_mb_neighbors       [4][4];
extern int8_t  blk_2x2_neighbor_idx       [4][4];
extern int8_t  blk_2x2_neighbor_coords [4][4][2];

extern int     neighbor_tables_initialized;
void           init_neighbor_tables(Undo264Context *ctx);





typedef struct {
    int cbp_luma;
    int cbp_chroma;

    union {
        struct {
            int16_t luma_4x4_coeffs[16][16]; /* Intra 4x4 */
            int16_t luma_8x8_coeffs[4][64];  /* Intra 8x8 */
            // we need both because 8x8 mode will read into the 4x4 coeffs first, then remap
        };
        struct { /* Intra 16x16 */
            int16_t luma_16x16_DC[16];
            int16_t luma_16x16_AC[16][15];
        };
    };
    int16_t chroma_DC[2][4];
    int16_t chroma_AC[2][4][15];

} MacroblockResiduals ;


/**
 * Small struct containing only the small information about a macroblock, needed for further decoding
 */
typedef struct MacroblockMetadata {
    int32_t mb_type;
    int8_t  mb_qp_delta;
    int32_t sub_mb_type[4];
    int8_t  coded_block_flag[14][16];
    int8_t  mb_skip_flag;
    uint8_t QPY;
    uint8_t QPC[2];
    uint8_t t_8x8_flag;
    uint8_t cbp_luma;
    uint8_t cbp_chroma;
    int16_t mvd[2][16][2];
    uint8_t intra_chroma_pred_mode;
    uint8_t intra_NxN_pred_mode[16]; // 8x8 will just use first 4 values
    uint8_t intra_16x16_pred_mode;

} MacroblockMetadata ;


typedef struct Macroblock {
    struct Picture *p_pic;

    uint32_t slice_type;
    int table_idx;
    int mb_type;
    int mbAddr;
    int mb_y, mb_x;
    int mb_width_c;
    int mb_height_c;
    int num_parts;
    int num_sub_parts;

    int has_mb_a, has_mb_b, has_mb_c, has_mb_d;
    int mb_a_off, mb_b_off, mb_c_off, mb_d_off;

    int32_t mb_qp_delta, QPY, QPC[2];
    int     t_8x8_flag;

    MacroblockResiduals residuals;


    union {
        struct {
            I_MbInfo mb_info;
        } i ;
        struct {
            PB_MbInfo mb_info;
            PB_MbInfo sub_mb_info[4];
            uint8_t   ref_idx[2][4];
            int16_t   mvd[2][4][4][2];
        } pb ;
    } u ;
} Macroblock ;





typedef struct Neighbor {
    int mb_off;
    int8_t idx;
    Coord c;
    bool av;
} Neighbor ;

typedef struct Neighbors {
    Neighbor a, b, c, d;
} Neighbors ;


static ALWAYS_INLINE Neighbor derive_a_neighbor_4x4(Macroblock *mb, int blkIdx, Undo264Context *ctx) {
    if (!neighbor_tables_initialized) {
        init_neighbor_tables(ctx);
    }
    Neighbor n = {0};

    n.mb_off = blk_4x4_mb_neighbors[blkIdx][0];
    n.idx    = blk_4x4_neighbor_idx[blkIdx][0];
    n.c      = (Coord){blk_4x4_neighbor_coords[blkIdx][0][1], blk_4x4_neighbor_coords[blkIdx][0][0]};
    n.av     = n.mb_off == 0 || mb->has_mb_a;

    return n;
}
static ALWAYS_INLINE Neighbor derive_b_neighbor_4x4(Macroblock *mb, int blkIdx, Undo264Context *ctx) {
    if (!neighbor_tables_initialized) {
        init_neighbor_tables(ctx);
    }
    Neighbor n = {0};

    n.mb_off = blk_4x4_mb_neighbors[blkIdx][1];
    n.idx    = blk_4x4_neighbor_idx[blkIdx][1];
    n.c      = (Coord){blk_4x4_neighbor_coords[blkIdx][1][1], blk_4x4_neighbor_coords[blkIdx][1][0]};
    n.av     = n.mb_off == 0 || mb->has_mb_b;

    return n;
}
static ALWAYS_INLINE Neighbor derive_c_neighbor_4x4(Macroblock *mb, int blkIdx, Undo264Context *ctx) {
    if (!neighbor_tables_initialized) {
        init_neighbor_tables(ctx);
    }
    Neighbor n = {0};

    int mb_b_off = blk_4x4_mb_neighbors[blkIdx][1];
    int mb_c_off = blk_4x4_mb_neighbors[blkIdx][2];

    n.mb_off = blk_4x4_mb_neighbors[blkIdx][2];
    n.idx    = blk_4x4_neighbor_idx[blkIdx][2];
    n.c      = (Coord){blk_4x4_neighbor_coords[blkIdx][2][1], blk_4x4_neighbor_coords[blkIdx][2][0]};
    n.av     = (mb_c_off == 0 || mb->has_mb_c || ((mb_b_off == 0 || mb->has_mb_b) && blkIdx!=3 && blkIdx!=7 && blkIdx!=11 && blkIdx!=15))
        && n.idx != -1;
    n.av = (mb_c_off == 0 || (mb_c_off == mb_b_off && mb->has_mb_b) || (mb_c_off == mb_b_off+1 && mb->has_mb_c));

    return n;
}
static ALWAYS_INLINE Neighbor derive_d_neighbor_4x4(Macroblock *mb, int blkIdx, Undo264Context *ctx) {
    if (!neighbor_tables_initialized) {
        init_neighbor_tables(ctx);
    }
    Neighbor n = {0};

    int mb_a_off = blk_4x4_mb_neighbors[blkIdx][0];
    int mb_b_off = blk_4x4_mb_neighbors[blkIdx][1];

    n.mb_off = blk_4x4_mb_neighbors[blkIdx][3];
    n.idx    = blk_4x4_neighbor_idx[blkIdx][3];
    n.c      = (Coord){blk_4x4_neighbor_coords[blkIdx][3][1], blk_4x4_neighbor_coords[blkIdx][3][0]};
    n.av     = (mb_a_off == 0 || mb->has_mb_a) && (mb_b_off == 0 || mb->has_mb_b) && (mb_a_off == 0 || mb_b_off == 0 || mb->has_mb_d);

    return n;
}


static ALWAYS_INLINE Neighbors derive_neighbors_4x4(Macroblock *mb, int blkIdx, Undo264Context *ctx) {
    if (!neighbor_tables_initialized) {
        init_neighbor_tables(ctx);
    }
    Neighbors n = {0};

    n.a = derive_a_neighbor_4x4(mb, blkIdx, ctx);
    n.b = derive_b_neighbor_4x4(mb, blkIdx, ctx);
    n.c = derive_c_neighbor_4x4(mb, blkIdx, ctx);
    n.d = derive_d_neighbor_4x4(mb, blkIdx, ctx);

    return n;
}


static ALWAYS_INLINE Neighbors derive_neighbors_2x2(Macroblock *mb, int blkIdx, Undo264Context *ctx) {
    if (!neighbor_tables_initialized) {
        init_neighbor_tables(ctx);
    }

    Neighbors n = {0};


    n.a.mb_off = blk_2x2_mb_neighbors[blkIdx][0];
    n.b.mb_off = blk_2x2_mb_neighbors[blkIdx][1];
    n.c.mb_off = blk_2x2_mb_neighbors[blkIdx][2];
    n.d.mb_off = blk_2x2_mb_neighbors[blkIdx][3];

    n.a.idx = blk_2x2_neighbor_idx[blkIdx][0];
    n.b.idx = blk_2x2_neighbor_idx[blkIdx][1];
    n.c.idx = blk_2x2_neighbor_idx[blkIdx][2];
    n.d.idx = blk_2x2_neighbor_idx[blkIdx][3];

    n.a.c = (Coord){blk_2x2_neighbor_coords[blkIdx][0][1], blk_2x2_neighbor_coords[blkIdx][0][0]};
    n.b.c = (Coord){blk_2x2_neighbor_coords[blkIdx][1][1], blk_2x2_neighbor_coords[blkIdx][1][0]};
    n.c.c = (Coord){blk_2x2_neighbor_coords[blkIdx][2][1], blk_2x2_neighbor_coords[blkIdx][2][0]};
    n.d.c = (Coord){blk_2x2_neighbor_coords[blkIdx][3][1], blk_2x2_neighbor_coords[blkIdx][3][0]};

    n.a.av = n.a.mb_off == 0 || mb->has_mb_a;
    n.b.av = n.b.mb_off == 0 || mb->has_mb_b;
    n.d.av = n.a.av && n.b.av;
    n.c.av = (n.c.mb_off == 0 || mb->has_mb_c || (n.b.av && blkIdx!=1))
        && n.c.idx != -1;

    return n;
}

static ALWAYS_INLINE void derive_macroblock_neighbors(Macroblock *mb, int first_mb_in_slice, Undo264Context *ctx) {
    int mb_addr = mb->mbAddr;
    int mb_width = ctx->ps->sps->pic_width_in_mbs;

    if (mb_addr % mb_width != 0 && mb_addr-1 >= first_mb_in_slice) {
        mb->has_mb_a = 1;
        mb->mb_a_off = - 1;
    } else { // top left, can't have A neighbor
        mb->has_mb_a = 0;
        mb->mb_a_off = 0;
    }
    if (mb_addr / mb_width >= 1 && mb_addr-mb_width >= first_mb_in_slice) {
        mb->has_mb_b = 1;
        mb->mb_b_off = - mb_width;
    } else { // top row, can't have B neighbor
        mb->has_mb_b = 0;
        mb->mb_b_off = 0;
    }
    if (mb_addr / mb_width >= 1 &&
        (mb_addr+1) % mb_width != 0 && mb_addr-mb_width+1 >= first_mb_in_slice) {
        mb->has_mb_c = 1;
        mb->mb_c_off = - mb_width + 1;
        } else { // top row or right column, can't have C neighbor
            mb->has_mb_c = 0;
            mb->mb_c_off = 0;
        }
    if (mb_addr / mb_width >= 1 &&
        mb_addr % mb_width != 0 && mb_addr-mb_width-1 >= first_mb_in_slice) {
        mb->has_mb_d = 1;
        mb->mb_d_off = - mb_width - 1;
        } else { // top row or left column, can't have D neighbor
            mb->has_mb_d = 0;
            mb->mb_d_off = 0;
        }
}

static ALWAYS_INLINE Macroblock *make_mb(int mbAddr, Undo264Context *ctx) {
    Macroblock *mb = calloc(1, sizeof(Macroblock));

    mb->mbAddr = mbAddr;
    mb->mb_y   = mbAddr / ctx->ps->sps->pic_width_in_mbs;
    mb->mb_x   = mbAddr % ctx->ps->sps->pic_width_in_mbs;

    return mb;
}

static ALWAYS_INLINE void reset_mb(Macroblock *mb, int mbAddr, Undo264Context *ctx) {
    memset(&ctx->luma_total_coeffs[mbAddr], 0, 16);
    memset(&ctx->cr_total_coeffs[mbAddr], 0, 16);
    memset(&ctx->cb_total_coeffs[mbAddr], 0, 16);


    memset(&mb->u, 0, sizeof(mb->u));
    memset(&mb->residuals, 0, sizeof(mb->residuals));

    mb->mbAddr = mbAddr;
    mb->mb_y   = mbAddr / ctx->ps->sps->pic_width_in_mbs;
    mb->mb_x   = mbAddr % ctx->ps->sps->pic_width_in_mbs;
    mb->p_pic  = ctx->curr_pic;
}


static void reset_motion_info(int mbAddr, Undo264Context *ctx) {
    MotionInfo *motion_info = ctx->curr_pic->motion_info[mbAddr];
    for (int i = 0; i < 16; i++) {
        motion_info[i].mvs[L0] = (MotionVector) {-1, 0, 0};
        motion_info[i].mvs[L1] = (MotionVector) {-1, 0, 0};
        motion_info[i].ref_pics[L0] = &EMPTY_PICTURE;
        motion_info[i].ref_pics[L1] = &EMPTY_PICTURE;
    }
    memset(&ctx->curr_pic->pred_flags[mbAddr][L0][0], false, 4);
    memset(&ctx->curr_pic->pred_flags[mbAddr][L1][0], false, 4);
}


void decode_i_macroblock(Macroblock *mb, struct Slice *slice, Undo264Context *ctx);
void decode_p_macroblock(Macroblock *mb, struct Slice *slice, Undo264Context *ctx);
void decode_b_macroblock(Macroblock *mb, struct Slice *slice, Undo264Context *ctx);

#endif //TOY_H264_MB_H