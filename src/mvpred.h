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

    if (debugging && mb->mbAddr == 9) {
        printf("A %s, B %s, C %s\n",
            a.av ? "av" : "not av",  b.av ? "av" : "not av",  c.av ? "av" : "not av");
        printf("A off %d, B off %d, C off %d\n",
            a.mb_off, b.mb_off, c.mb_off);
        printf("A idx %d, B idx %d, C idx %d\n",
            a.idx, b.idx, c.idx);
    }

    if (!c.av) {
        c = d;
    }

    if (!c.av && !b.av && a.av) {
        b = a;
        c = a;
    }

    MotionVector mv_a = {-1, 0, 0};
    MotionVector mv_b = {-1, 0, 0};
    MotionVector mv_c = {-1, 0, 0};

    if (a.av) mv_a = ctx->mvs_l0[mb->mbAddr + a.mb_off][a.idx];
    if (b.av) mv_b = ctx->mvs_l0[mb->mbAddr + b.mb_off][b.idx];
    if (c.av) mv_c = ctx->mvs_l0[mb->mbAddr + c.mb_off][c.idx];

    if (debugging && mb->mbAddr == 256) {
        printf(" A ref idx %d, B ref idx %d, C ref idx %d\n",
            mv_a.ref_idx, mv_b.ref_idx, mv_c.ref_idx);
    }

    int a_match = a.av && (mv_a.ref_idx == refIdx);
    int b_match = b.av && (mv_b.ref_idx == refIdx);
    int c_match = c.av && (mv_c.ref_idx == refIdx);
    if (a_match + b_match + c_match == 1) {
        if      (a_match) { mv.x = mv_a.x; mv.y = mv_a.y; }
        else if (b_match) { mv.x = mv_b.x; mv.y = mv_b.y; }
        else if (c_match) { mv.x = mv_c.x; mv.y = mv_c.y; }
    } else {
        mv.x = _median(mv_a.x, mv_b.x, mv_c.x);
        mv.y = _median(mv_a.y, mv_b.y, mv_c.y);
    }


    return mv;
}

static ALWAYS_INLINE void derive_p_skip_mv(Macroblock *mb, CodecContext *ctx) {
    MotionVector mv = {0, 0, 0};

    Neighbor a = derive_a_neighbor_4x4(mb, 0, ctx);
    Neighbor b = derive_b_neighbor_4x4(mb, 0, ctx);

    if (a.av && b.av) {
        MotionVector mv_a = ctx->mvs_l0[mb->mbAddr + a.mb_off][a.idx];
        MotionVector mv_b = ctx->mvs_l0[mb->mbAddr + b.mb_off][b.idx];
        bool a_is_zero = (mv_a.ref_idx == 0 && mv_a.x == 0 && mv_a.y == 0);
        bool b_is_zero = (mv_b.ref_idx == 0 && mv_b.x == 0 && mv_b.y == 0);
        if (!a_is_zero && !b_is_zero) {
            mv = get_median_mv(mb, 0, 0, 3, ctx);
        }
    }

    // broadcast the MV through the whole 4x4 MV block
    for (int i = 0; i < 16; i++) {
        ctx->mvs_l0[mb->mbAddr][i] = mv;
    }
    memset(&ctx->pred_flag_l0[mb->mbAddr][0], 1, 4);
}

static ALWAYS_INLINE void derive_p_16x16_mv(Macroblock *mb, CodecContext *ctx) {
    MotionVector mv = get_median_mv(mb, mb->u.pb.ref_idx_l0[0], 0, 3, ctx);

    // add delta and broadcast the MV through the whole 4x4 MV block
    for (int i = 0; i < 16; i++) {
        ctx->mvs_l0[mb->mbAddr][i].ref_idx = mv.ref_idx;
        ctx->mvs_l0[mb->mbAddr][i].x       = mv.x + mb->u.pb.mvd_l0[0][0][0];
        ctx->mvs_l0[mb->mbAddr][i].y       = mv.y + mb->u.pb.mvd_l0[0][0][1];
    }
    memset(&ctx->pred_flag_l0[mb->mbAddr][0], 1, 4);
}


static ALWAYS_INLINE void derive_p_16x8_mv(Macroblock *mb, CodecContext *ctx) {
    MotionVector mv1 = {mb->u.pb.ref_idx_l0[0], 0, 0};
    MotionVector mv2 = {mb->u.pb.ref_idx_l0[1], 0, 0};

    Neighbors n1 = derive_neighbors_4x4(mb, 0, ctx); // top neighbor of first partition
    Neighbors n2 = derive_neighbors_4x4(mb, 8, ctx); // left neighbor of second partition


    if (n1.b.av && ctx->mvs_l0[mb->mbAddr + n1.b.mb_off][n1.b.idx].ref_idx == mv1.ref_idx) {
        mv1.x = ctx->mvs_l0[mb->mbAddr + n1.b.mb_off][n1.b.idx].x;
        mv1.y = ctx->mvs_l0[mb->mbAddr + n1.b.mb_off][n1.b.idx].y;
    } else {
        mv1 = get_median_mv(mb, mv1.ref_idx, 0, 3, ctx);
    }
    for (int i = 0; i < 8; i++) {
        ctx->mvs_l0[mb->mbAddr][i].ref_idx = mv1.ref_idx;
        ctx->mvs_l0[mb->mbAddr][i].x       = mv1.x + mb->u.pb.mvd_l0[0][0][0];
        ctx->mvs_l0[mb->mbAddr][i].y       = mv1.y + mb->u.pb.mvd_l0[0][0][1];
    }


    if (n2.a.av && ctx->mvs_l0[mb->mbAddr + n2.a.mb_off][n2.a.idx].ref_idx == mv2.ref_idx) {
        mv2.x = ctx->mvs_l0[mb->mbAddr + n2.a.mb_off][n2.a.idx].x;
        mv2.y = ctx->mvs_l0[mb->mbAddr + n2.a.mb_off][n2.a.idx].y;
    } else {
        mv2 = get_median_mv(mb, mv2.ref_idx, 8, 11, ctx);
    }
    for (int i = 8; i < 16; i++) {
        ctx->mvs_l0[mb->mbAddr][i].ref_idx = mv2.ref_idx;
        ctx->mvs_l0[mb->mbAddr][i].x       = mv2.x + mb->u.pb.mvd_l0[1][0][0];
        ctx->mvs_l0[mb->mbAddr][i].y       = mv2.y + mb->u.pb.mvd_l0[1][0][1];
    }


    memset(&ctx->pred_flag_l0[mb->mbAddr][0], 1, 4);
}

static ALWAYS_INLINE void derive_p_8x16_mv(Macroblock *mb, CodecContext *ctx) {
    MotionVector mv1 = {mb->u.pb.ref_idx_l0[0], 0, 0};
    MotionVector mv2 = {mb->u.pb.ref_idx_l0[1], 0, 0};

	Neighbor a = derive_a_neighbor_4x4(mb, 0, ctx);
	Neighbor c = derive_c_neighbor_4x4(mb, 3, ctx);
    Neighbor d = derive_d_neighbor_4x4(mb, 2, ctx); /* let's cheat a little here, as we need the top-right macroblock
                                                             and using blkIdx=2 would have given us the bottom-right 4x4 block
                                                             of the macroblock above as the C neighbor */

    if (!c.av) {
        c = d;
    }

	if (debugging) {
		printf("%d %d %d %d\n", a.av, 1, c.av, d.av);
	}

    if (a.av && ctx->mvs_l0[mb->mbAddr + a.mb_off][a.idx].ref_idx == mv1.ref_idx) {
            mv1.x = ctx->mvs_l0[mb->mbAddr + a.mb_off][a.idx].x;
            mv1.y = ctx->mvs_l0[mb->mbAddr + a.mb_off][a.idx].y;
    } else {
        mv1 = get_median_mv(mb, mv1.ref_idx, 0, 1, ctx);
    }
    for (int i = 0; i < 8; i++) {
        int pos = ((i>>1)<<2) + (i&1);
        ctx->mvs_l0[mb->mbAddr][pos].ref_idx = mv1.ref_idx;
        ctx->mvs_l0[mb->mbAddr][pos].x       = mv1.x + mb->u.pb.mvd_l0[0][0][0];
        ctx->mvs_l0[mb->mbAddr][pos].y       = mv1.y + mb->u.pb.mvd_l0[0][0][1];
    }


    if (c.av && ctx->mvs_l0[mb->mbAddr + c.mb_off][c.idx].ref_idx == mv2.ref_idx) {
        mv2.x = ctx->mvs_l0[mb->mbAddr + c.mb_off][c.idx].x;
        mv2.y = ctx->mvs_l0[mb->mbAddr + c.mb_off][c.idx].y;
    } else {
        mv2 = get_median_mv(mb, mv2.ref_idx, 2, 3, ctx);
    }
    for (int i = 0; i < 8; i++) {
        int pos = ((i>>1)<<2) + 2 + (i&1);
        ctx->mvs_l0[mb->mbAddr][pos].ref_idx = mv2.ref_idx;
        ctx->mvs_l0[mb->mbAddr][pos].x       = mv2.x + mb->u.pb.mvd_l0[1][0][0];
        ctx->mvs_l0[mb->mbAddr][pos].y       = mv2.y + mb->u.pb.mvd_l0[1][0][1];
    }

    memset(&ctx->pred_flag_l0[mb->mbAddr][0], 1, 4);
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
        MotionVector mv = get_median_mv(mb, mb->u.pb.ref_idx_l0[partIdx], sub_part_idx, sub_part_idx+1, ctx);
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
        ctx->mvs_l0[mb->mbAddr][sub_part_idx].ref_idx = mv.ref_idx;
        ctx->mvs_l0[mb->mbAddr][sub_part_idx].x       = mv.x + mb->u.pb.mvd_l0[partIdx][subPart][0];
        ctx->mvs_l0[mb->mbAddr][sub_part_idx].y       = mv.y + mb->u.pb.mvd_l0[partIdx][subPart][1];
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
    memset(&ctx->pred_flag_l0[mb->mbAddr][0], 1, 4);
}


#endif //TOY_H264_MVPRED_H