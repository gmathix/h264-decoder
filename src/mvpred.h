//
// Created by gmathix on 5/1/26.
//

#ifndef TOY_H264_MVPRED_H
#define TOY_H264_MVPRED_H


#include "global.h"
#include "mb.h"
#include "mv.h"



static ALWAYS_INLINE MotionVector get_median_mv(Macroblock *mb, int refIdx, int idx_for_abd, int idx_for_c, CodecContext *ctx) {
    MotionVector mv = {refIdx, 0, 0};

    Neighbor a = derive_a_neighbor_4x4(mb, idx_for_abd, ctx);
    Neighbor b = derive_b_neighbor_4x4(mb, idx_for_abd, ctx);
    Neighbor c = derive_c_neighbor_4x4(mb, idx_for_c, ctx);
    Neighbor d = derive_d_neighbor_4x4(mb, idx_for_abd, ctx);

    if (a.av && b.av) {
        if (!c.av) {
            c.idx = d.idx;
            c.mb_off = d.mb_off;
        }
        MotionVector mv_a = ctx->mvs_l0[mb->mbAddr + a.mb_off][a.idx];
        MotionVector mv_b = ctx->mvs_l0[mb->mbAddr + b.mb_off][b.idx];
        MotionVector mv_c = ctx->mvs_l0[mb->mbAddr + c.mb_off][c.idx];

        bool a_is_zero = (mv_a.ref_idx == 0 && mv_a.x == 0 && mv_a.y == 0);
        bool b_is_zero = (mv_b.ref_idx == 0 && mv_b.x == 0 && mv_b.y == 0);

        if (!a_is_zero && !b_is_zero) {
            if ((_abs(mv_a.ref_idx)==0 + _abs(mv_b.ref_idx)==0 + _abs(mv_c.ref_idx)==0) == 1) {
                if      (mv_a.ref_idx == 0) { mv.x = mv_a.x; mv.y = mv_a.y; }
                else if (mv_b.ref_idx == 0) { mv.x = mv_b.x; mv.y = mv_b.y; }
                else if (mv_c.ref_idx == 0) { mv.x = mv_c.x; mv.y = mv_c.y; }
            } else {
                mv.x = _median(mv_a.x, mv_b.x, mv_c.x);
                mv.y = _median(mv_a.y, mv_b.y, mv_c.y);
            }
        }
    }

    return mv;
}

static ALWAYS_INLINE void derive_p_skip_mv(Macroblock *mb, CodecContext *ctx) {
    MotionVector mv = get_median_mv(mb, 0, 0, 0, ctx);

    // broadcast the MV through the whole 4x4 MV block
    for (int i = 0; i < 16; i++) {
        ctx->mvs_l0[mb->mbAddr][i].ref_idx = mv.ref_idx;
        ctx->mvs_l0[mb->mbAddr][i].x       = mv.x;
        ctx->mvs_l0[mb->mbAddr][i].y       = mv.y;
    }
}

static ALWAYS_INLINE void derive_p_16x16_mv(Macroblock *mb, CodecContext *ctx) {
    MotionVector mv = get_median_mv(mb, mb->u.pb.ref_idx_l0[0], 0, 0, ctx);

    // add delta and broadcast the MV through the whole 4x4 MV block
    for (int i = 0; i < 16; i++) {
        ctx->mvs_l0[mb->mbAddr][i].ref_idx = mv.ref_idx;
        ctx->mvs_l0[mb->mbAddr][i].x       = mv.x + mb->u.pb.mvd_l0[0][0][0];
        ctx->mvs_l0[mb->mbAddr][i].y       = mv.y + mb->u.pb.mvd_l0[0][0][1];
    }
}


static ALWAYS_INLINE void derive_p_16x8_mv(Macroblock *mb, CodecContext *ctx) {
    MotionVector mv1 = {mb->u.pb.ref_idx_l0[0], 0, 0};
    MotionVector mv2 = {mb->u.pb.ref_idx_l1[1], 0, 0};

    Neighbors n1 = derive_neighbors_4x4(mb, 0, ctx); // top neighbor of first partition
    Neighbors n2 = derive_neighbors_4x4(mb, 8, ctx); // left neighbor of second partition

    if (n1.b.av) {
        if (ctx->mvs_l0[mb->mbAddr + n1.b.mb_off][n1.b.idx].ref_idx == mv1.ref_idx) {
            mv1.x = ctx->mvs_l0[mb->mbAddr + n1.b.mb_off][n1.b.idx].x;
            mv1.y = ctx->mvs_l0[mb->mbAddr + n1.b.mb_off][n1.b.idx].y;
        } else {
            mv1 = get_median_mv(mb, mv1.ref_idx, 0, 3, ctx);
        }
    }
    if (n2.a.av) {
        if (ctx->mvs_l0[mb->mbAddr + n2.a.mb_off][n1.a.idx].ref_idx == mv2.ref_idx) {
            mv2.x = ctx->mvs_l0[mb->mbAddr + n2.a.mb_off][n2.a.idx].x;
            mv2.y = ctx->mvs_l0[mb->mbAddr + n2.a.mb_off][n2.a.idx].y;
        } else {
            mv2 = get_median_mv(mb, mv2.ref_idx, 8, 11, ctx);
        }
    }

    // add delta and broadcast both MVs through the corresponding 4x2 blocks
    for (int i = 0; i < 8; i++) {
        ctx->mvs_l0[mb->mbAddr][i].ref_idx = mv1.ref_idx;
        ctx->mvs_l0[mb->mbAddr][i].x       = mv1.x + mb->u.pb.mvd_l0[0][0][0];
        ctx->mvs_l0[mb->mbAddr][i].y       = mv1.y + mb->u.pb.mvd_l0[0][0][1];
    }
    for (int i = 8; i < 16; i++) {
        ctx->mvs_l0[mb->mbAddr][i].ref_idx = mv2.ref_idx;
        ctx->mvs_l0[mb->mbAddr][i].x       = mv2.x + mb->u.pb.mvd_l0[1][0][0];
        ctx->mvs_l0[mb->mbAddr][i].y       = mv2.y + mb->u.pb.mvd_l0[1][0][1];
    }
}

static ALWAYS_INLINE void derive_p_8x16_mv(Macroblock *mb, CodecContext *ctx) {
    MotionVector mv1 = {mb->u.pb.ref_idx_l0[0], 0, 0};
    MotionVector mv2 = {mb->u.pb.ref_idx_l0[1], 0, 0};

    Neighbors n1 = derive_neighbors_4x4(mb, 0, ctx);
    Neighbors n2 = derive_neighbors_4x4(mb, 3, ctx); /* let's cheat a little here, as we need the top-right macroblock
                                                            and using blkIdx=2 would have given us the bottom-right 4x4 block
                                                            of the macroblock above as the C neighbor */

    if (n1.a.av) {
        if (ctx->mvs_l0[mb->mbAddr + n1.a.mb_off][n1.a.idx].ref_idx == mv1.ref_idx) {
            mv1.x = ctx->mvs_l0[mb->mbAddr + n1.a.mb_off][n1.a.idx].x;
            mv1.y = ctx->mvs_l0[mb->mbAddr + n1.a.mb_off][n1.a.idx].y;
        } else {
            mv1 = get_median_mv(mb, mv1.ref_idx, 0, 1, ctx);
        }
    }
    if (n2.c.av) {
        if (ctx->mvs_l0[mb->mbAddr + n1.b.mb_off][n1.b.idx].ref_idx == mv1.ref_idx) {
            mv2.x = ctx->mvs_l0[mb->mbAddr + n2.c.mb_off][n2.c.idx].x;
            mv2.y = ctx->mvs_l0[mb->mbAddr + n2.c.mb_off][n2.c.idx].y;
        } else {
            mv2 = get_median_mv(mb, mv2.ref_idx, 2, 3, ctx);
        }
    }

    // add delta and broadcast both MVs through the corresponding 2x4 blocks
    for (int i = 0; i < 8; i++) {
        int pos = ((i>>1)<<2) + (i&1);
        ctx->mvs_l0[mb->mbAddr][pos].ref_idx = mv1.ref_idx;
        ctx->mvs_l0[mb->mbAddr][pos].x       = mv1.x + mb->u.pb.mvd_l0[0][0][0];
        ctx->mvs_l0[mb->mbAddr][pos].y       = mv1.y + mb->u.pb.mvd_l0[0][0][1];
    }
    for (int i = 8; i < 16; i++) {
        int pos = ((i>>1)<<2) + (i&1);
        ctx->mvs_l0[mb->mbAddr][pos].ref_idx = mv2.ref_idx;
        ctx->mvs_l0[mb->mbAddr][pos].x = mv2.x + mb->u.pb.mvd_l0[1][0][0];
        ctx->mvs_l0[mb->mbAddr][pos].y = mv2.y + mb->u.pb.mvd_l0[1][0][1];
    }
}

static ALWAYS_INLINE void derive_p_sub_8x8_mv(Macroblock *mb, int partIdx, CodecContext *ctx) {
    int part_4x4_idx = partIdx/2*8 + (partIdx%2)*2;
    MotionVector mv = get_median_mv(
        mb, mb->u.pb.ref_idx_l0[partIdx],
        part_4x4_idx,
         part_4x4_idx + 1, ctx);

    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
            ctx->mvs_l0[mb->mbAddr][part_4x4_idx + i*4 + j].ref_idx = mv.ref_idx;
            ctx->mvs_l0[mb->mbAddr][part_4x4_idx + i*4 + j].x       = mv.x + mb->u.pb.mvd_l0[partIdx][0][0];
            ctx->mvs_l0[mb->mbAddr][part_4x4_idx + i*4 + j].y       = mv.y + mb->u.pb.mvd_l0[partIdx][0][1];
        }
    }
}

static ALWAYS_INLINE void derive_p_sub_8x4_mv(Macroblock *mb, int partIdx, CodecContext *ctx) {
    int part_4x4_idx = partIdx/2*8 + (partIdx%2)*2;

    for (int subPart = 0; subPart < 2; subPart++) {
        int sub_part_idx = part_4x4_idx + subPart*4;
        MotionVector mv = get_median_mv(mb, mb->u.pb.ref_idx_l0[partIdx], part_4x4_idx, part_4x4_idx+1, ctx);
        for (int i = 0; i < 2; i++) {
            ctx->mvs_l0[mb->mbAddr][sub_part_idx + i].ref_idx = mv.ref_idx;
            ctx->mvs_l0[mb->mbAddr][sub_part_idx + i].x       = mv.x + mb->u.pb.mvd_l0[partIdx][subPart][0];
            ctx->mvs_l0[mb->mbAddr][sub_part_idx + i].y       = mv.y + mb->u.pb.mvd_l0[partIdx][subPart][1];
        }
    }
}

static ALWAYS_INLINE void derive_p_sub_4x8_mv(Macroblock *mb, int partIdx, CodecContext *ctx) {
    int part_4x4_idx = partIdx/2*8 + (partIdx%2)*2;

    for (int subPart = 0; subPart < 2; subPart++) {
        int sub_part_idx = part_4x4_idx + subPart;
        MotionVector mv = get_median_mv(mb, mb->u.pb.ref_idx_l0[partIdx], sub_part_idx, sub_part_idx, ctx);
        for (int i = 0; i < 2; i++) {
            ctx->mvs_l0[mb->mbAddr][sub_part_idx + i*4].ref_idx = mv.ref_idx;
            ctx->mvs_l0[mb->mbAddr][sub_part_idx + i*4].x       = mv.x + mb->u.pb.mvd_l0[partIdx][subPart][0];
            ctx->mvs_l0[mb->mbAddr][sub_part_idx + i*4].y       = mv.y + mb->u.pb.mvd_l0[partIdx][subPart][1];
        }
    }
}

static ALWAYS_INLINE void derive_p_sub_4x4_mv(Macroblock *mb, int partIdx, CodecContext *ctx) {
    int part_4x4_idx = partIdx/2*8 + (partIdx%2)*2;
    for (int subPart = 0; subPart < 4; subPart++) {
        int sub_part_idx = part_4x4_idx + subPart/2*4 + subPart%2;
        MotionVector mv = get_median_mv(mb, mb->u.pb.ref_idx_l0[partIdx], sub_part_idx, sub_part_idx, ctx);
        ctx->mvs_l0[mb->mbAddr][part_4x4_idx].ref_idx = mv.ref_idx;
        ctx->mvs_l0[mb->mbAddr][part_4x4_idx].x       = mv.x + mb->u.pb.mvd_l0[partIdx][subPart][0];
        ctx->mvs_l0[mb->mbAddr][part_4x4_idx].y       = mv.y + mb->u.pb.mvd_l0[partIdx][subPart][1];
    }
}

static ALWAYS_INLINE void derive_p_8x8_mv(Macroblock *mb, CodecContext *ctx) {
    for (int part = 0; part < 4; part++) {
        if (mb->u.pb.sub_mb_info[part].type & SUB_MB_TYPE_8x8) {
            derive_p_sub_8x8_mv(mb, part, ctx);
        } else if (mb->u.pb.sub_mb_info[part].type & SUB_MB_TYPE_8x4) {
            derive_p_sub_8x4_mv(mb, part, ctx);
        } else if (mb->u.pb.sub_mb_info[part].type & SUB_MB_TYPE_4x8) {
            derive_p_sub_4x8_mv(mb, part, ctx);
        } else if (mb->u.pb.sub_mb_info[part].type & SUB_MB_TYPE_4x4) {
            derive_p_sub_4x4_mv(mb, part, ctx);
        }
    }
}


#endif //TOY_H264_MVPRED_H