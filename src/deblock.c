//
// Created by gmathix on 5/12/26.
//



#include "deblock.h"


#include "dpb.h"
#include "mb.h"
#include "motion_info.h"


#include "util/mbutil.h"


const uint8_t alpha_table[52] = {
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   4,   4,   5,   6,   7,   8,   9,  10,  12,  13,
     15,  17,  20,  22,  25,  28,  32,  36,  40,  45,  50,  56,  63,
     71,  80,  90, 101, 113, 127, 144, 162, 182, 203, 226, 255, 255,
};

const uint8_t beta_table[52] = {
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  2,  2,  2,  3,  3,  3,  3,  4,  4,  4,
     6,  6,  7,  7,  8,  8,  9,  9, 10, 10, 11, 11, 12,
    12, 13, 13, 14, 14, 15, 15, 16, 16, 17, 17, 18, 18,
};

const uint8_t treshold_table[3][52] = {
    {  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
       0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,
       1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  3,  3,
       3,  4,  4,  4,  5,  6,  6,  7,  8,  9, 10, 11, 13,
    },

    {  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
       0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,  1,
       1,  1,  1,  1,  1,  2,  2,  2,  2,  3,  3,  3,  4,
       4,  5,  5,  6,  7,  8,  8, 10, 11, 12, 13, 15, 17,
    },

    {  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
       0,  0,  0,  0,  1,  1,  1,  1,  1,  1,  1,  1,  1,
       1,  2,  2,  2,  2,  3,  3,  3,  4,  4,  4,  5,  6,
       6,  7,  8,  9, 10, 11, 13, 14, 16, 18, 20, 23, 25,
    },
};




static ALWAYS_INLINE bool same_ref_pics(int refL0_0, int refL0_1, int refL1_0, int refL1_1, Undo264Context *ctx) {
    // when refLX_X is -1 (no ref) we just get EMPTY_PIC which has dpb_pic_id = 0
    Picture *picL0_0 = ctx->dpb->lists[L0][1+refL0_0];
    Picture *picL0_1 = ctx->dpb->lists[L0][1+refL0_1];
    Picture *picL1_0 = ctx->dpb->lists[L1][1+refL1_0];
    Picture *picL1_1 = ctx->dpb->lists[L1][1+refL1_1];
    int hash0 = ((picL0_0->dpb_pic_id != 0) * (1 << picL0_0->dpb_pic_id)) |
                ((picL1_0->dpb_pic_id != 0) * (1 << picL1_0->dpb_pic_id));
    int hash1 = ((picL0_1->dpb_pic_id != 0) * (1 << picL0_1->dpb_pic_id)) |
                ((picL1_1->dpb_pic_id != 0) * (1 << picL1_1->dpb_pic_id));
    return hash0 == hash1;
}

static ALWAYS_INLINE bool same_ref_pics_one_block(int refL0, int refL1, Undo264Context *ctx) {
    return ctx->dpb->lists[L0][1+refL0]->dpb_pic_id == ctx->dpb->lists[L1][1+refL1]->dpb_pic_id;
}

static ALWAYS_INLINE bool mv_diff_g4(MotionVector mv1, MotionVector mv2) {
    return (_abs(mv1.x - mv2.x) >= 4) || (_abs(mv1.y - mv2.y) >= 4);
}



/**
 * derive the 4 bS values needed for a vertical or horizontal edge
 * @param mbAddr     address if the macroblock to deblock
 * @param mbAddrN    address of the macroblock containing the sample p0 (neighbor of mbAddr)
 * @param blkIdx     initial 4x4 block index in current mb
 * @param blkIdxN    initial 4x4 block index in neighbor mb
 * @param blkIdx8x8  initial 8x8 block_index in current mb
 * @param blkIdx8x8N initial 8x8 block index in neighbor mb
 */
void derive_edge_bS_list(int mbAddr, int mbAddrN, int blkIdx, int blkIdxN, int blkIdx8x8, int blkIdx8x8N,
    bool mb_edge, bool vertical, uint8_t bS_list[4], Undo264Context *ctx) {

    MacroblockMetadata meta   = ctx->mb_metadata[mbAddr];
    MacroblockMetadata meta_n = ctx->mb_metadata[mbAddrN];


    int blkAdd    = 1 + vertical*3; // 1 for horizontal, 4 for vertical
    int blkAdd8x8 = 1 + vertical;
    int curr_bS   = 0;

    for (int i = 0; i < 4; i++) {
        int idx       = blkIdx      + i*blkAdd;
        int idx_n     = blkIdxN     + i*blkAdd;
        int idx_8x8   = blkIdx8x8   + (i/2)*blkAdd8x8;
        int idx_n_8x8 = blkIdx8x8N  + (i/2)*blkAdd8x8;

        MotionVector mvL0_0 = ctx->curr_pic->motion_info[mbAddr][idx].mvs[0];
        MotionVector mvL0_1 = ctx->curr_pic->motion_info[mbAddrN][idx_n].mvs[0];
        MotionVector mvL1_0 = ctx->curr_pic->motion_info[mbAddr][idx].mvs[1];
        MotionVector mvL1_1 = ctx->curr_pic->motion_info[mbAddrN][idx_n].mvs[1];

        /*
         * bS = 4
         * <=> edge is a macroblock edge and either or both of mb and mb_n are intra macroblocks
         */
        curr_bS += (mb_edge && (IS_INTRA(meta.mb_type) || IS_INTRA(meta_n.mb_type))) * 4;

        /*
         * bS = 3
         * <=> either or both of mb and mb_n are intra macroblocks
         */
        curr_bS += ((curr_bS == 0) &&
        	(IS_INTRA(meta.mb_type) || IS_INTRA(meta_n.mb_type))) * 3;

        /*
         * bS = 2
         * <=> the corresponding 4x4 or 8x8 transform blocks (depending on transform_8x8_flag) have non-zero coeff levels
         */
        curr_bS += ((curr_bS == 0) &&
        	((meta.t_8x8_flag    && (meta.cbp_luma   & (1 << idx_8x8))) ||
             (meta_n.t_8x8_flag  && (meta_n.cbp_luma & (1 << idx_n_8x8))) ||
             (!meta.t_8x8_flag   && (ctx->luma_total_coeffs[mbAddr][idx] > 0)) ||
             (!meta_n.t_8x8_flag && (ctx->luma_total_coeffs[mbAddrN][idx_n] > 0)))) * 2;

        /* bS = 1
         * <=> too much writing, 8.7.2.1
         */
    	int flag0_0 = ctx->curr_pic->pred_flags[mbAddr][0][idx_8x8];
    	int flag1_0 = ctx->curr_pic->pred_flags[mbAddr][1][idx_8x8];
    	int flag0_1 = ctx->curr_pic->pred_flags[mbAddrN][0][idx_n_8x8];
    	int flag1_1 = ctx->curr_pic->pred_flags[mbAddrN][1][idx_n_8x8];
        int nbMV0   = flag0_0 + flag1_0;
        int nbMV1   = flag0_1 + flag1_1;
        MotionVector singleMV0 = flag0_0 ? mvL0_0 : mvL1_0;
        MotionVector singleMV1 = flag0_1 ? mvL0_1 : mvL1_1;

        bool same_pics = same_ref_pics(mvL0_0.ref_idx, mvL0_1.ref_idx, mvL1_0.ref_idx, mvL1_1.ref_idx, ctx);

        curr_bS += ((curr_bS == 0) &&
        	(
        	    (!same_pics) ||
                (nbMV0 != nbMV1) || // different number of MVs
                ((nbMV0 == 1 && nbMV1 == 1) && mv_diff_g4(singleMV0, singleMV1)) || // one MV on each side and abs(mv0-mv1) >= 4 for x or y
                ((nbMV0 == 2 && nbMV1 == 2) && !same_ref_pics_one_block(mvL0_1.ref_idx, mvL1_1.ref_idx, ctx) &&
                     ((mvL0_0.ref_idx == mvL0_1.ref_idx && (mv_diff_g4(mvL0_0, mvL0_1) || mv_diff_g4(mvL1_0, mvL1_1))) ||
                      (mvL0_0.ref_idx != mvL0_1.ref_idx && (mv_diff_g4(mvL0_0, mvL1_1) || mv_diff_g4(mvL1_0, mvL0_1))))) ||
                ((nbMV0 == 2 && nbMV1 == 2) && same_ref_pics_one_block(mvL0_0.ref_idx, mvL1_0.ref_idx, ctx) &&
                     ((mv_diff_g4(mvL0_0, mvL0_1) || mv_diff_g4(mvL1_0, mvL1_1)) &&
                      (mv_diff_g4(mvL0_1, mvL1_0) || mv_diff_g4(mvL1_1, mvL0_0))))
            )) * 1;

        bS_list[i] = curr_bS;
        curr_bS = 0;
    }
}


void derive_edge_treshold_luma(Picture *pic, int mbAddr, int mbAddrN, uint8_t bS, int y, int x, bool vertical,
    uint8_t *alpha, uint8_t *beta, int filter_flags[4], uint8_t *indexA,
    uint8_t samples[24][24], Undo264Context *ctx) {


    int qpAv = (ctx->mb_metadata[mbAddr].QPY + ctx->mb_metadata[mbAddrN].QPY + 1) >> 1;



    *indexA    = _clip3(0, 51, qpAv + pic->sh->slice_alpha_c0_offset_div2 * 2);
    int indexB = _clip3(0, 51, qpAv + pic->sh->slice_beta_offset_div2 * 2);

    *alpha = alpha_table[*indexA]; // assume bitDepth = 8
    *beta  = beta_table[indexB];



    if (vertical) {
        for (int i = 0; i < 4; i++) {
            filter_flags[i] =
                (bS != 0) &&
                (_abs(samples[y][x-1] - samples[y][x]) < *alpha) &&
                (_abs(samples[y][x-2] - samples[y][x-1]) < *beta) &&
                (_abs(samples[y][x+1] - samples[y][x]) < *beta);
            y++;
        }
    } else {
        for (int i = 0; i < 4; i++) {
            filter_flags[i] =
                (bS != 0) &&
                (_abs(samples[y-1][x] - samples[y][x]) < *alpha) &&
                (_abs(samples[y-2][x] - samples[y-1][x]) < *beta) &&
                (_abs(samples[y+1][x] - samples[y][x]) < *beta);
            x++;
        }
    }
}

void derive_edge_treshold_chroma(Picture *pic, int mbAddr, int mbAddrN, uint8_t bS, int y, int x, bool vertical,
    uint8_t *alpha, uint8_t *beta, int filter_flags[2], uint8_t *indexA,
    uint8_t samples[16][16], Undo264Context *ctx) {


    int qpAv = (ctx->mb_metadata[mbAddr].QPC + ctx->mb_metadata[mbAddrN].QPC + 1) >> 1;



    *indexA    = _clip3(0, 51, qpAv + pic->sh->slice_alpha_c0_offset_div2 * 2);
    int indexB = _clip3(0, 51, qpAv + pic->sh->slice_beta_offset_div2 * 2);

    *alpha = alpha_table[*indexA]; // assume bitDepth = 8
    *beta  = beta_table[indexB];


    if (vertical) {
        for (int i = 0; i < 2; i++) {
            filter_flags[i] =
                (bS != 0) &&
                (_abs(samples[y][x-1] - samples[y][x]) < *alpha) &&
                (_abs(samples[y][x-2] - samples[y][x-1]) < *beta) &&
                (_abs(samples[y][x+1] - samples[y][x]) < *beta);
            y++;
        }
    } else {
        for (int i = 0; i < 2; i++) {
            filter_flags[i] =
                (bS != 0) &&
                (_abs(samples[y-1][x] - samples[y][x]) < *alpha) &&
                (_abs(samples[y-2][x] - samples[y-1][x]) < *beta) &&
                (_abs(samples[y+1][x] - samples[y][x]) < *beta);
            x++;
        }
    }
}


void filter_4p_vert_edge_low_bS_luma(int y, int x, const int filter_flags[4],
    uint8_t bS, uint8_t indexA, uint8_t beta, uint8_t samples[24][24]) {

    int treshold = treshold_table[bS-1][indexA];
    int aP, aQ, t, delta;

    int p0, p1, p2, q0, q1, q2;

    for (int i = 0; i < 4; i++) {
        if (filter_flags[i]) {
            p0 = samples[y][x-1];  p1 = samples[y][x-2];  p2 = samples[y][x-3];
            q0 = samples[y][x];    q1 = samples[y][x+1];  q2 = samples[y][x+2];

            aP = _abs(p2 - p0);
            aQ = _abs(q2 - q0);

            t = treshold + ((aP < beta) + (aQ < beta));

            delta = _clip3(-t, t,
                (((q0 - p0) * (1 << 2)) + (p1 - q1) + 4) >> 3);

            /*p1*/ samples[y][x-2] += (aP < beta) * _clip3(-treshold, treshold,
                  (p2 + ((p0 + q0 + 1) >> 1) - (p1 << 1)) >> 1);
            /*p0*/ samples[y][x-1] = _clip1y(p0 + delta, 8);

            /*q0*/ samples[y][x]   = _clip1y(q0 - delta, 8);
            /*q1*/ samples[y][x+1] += (aQ < beta) * _clip3(-treshold, treshold,
                  (q2 + ((p0 + q0 + 1) >> 1) - (q1 << 1)) >> 1);
        }

        y++;
    }
}

void filter_4p_hor_edge_low_bS_luma(int y, int x, const int filter_flags[4],
    uint8_t bS, uint8_t indexA, uint8_t beta, uint8_t samples[24][24]) {

    int treshold = treshold_table[bS-1][indexA];
    int aP, aQ, t, delta;

    int p0, p1, p2, q0, q1, q2;


    for (int i = 0; i < 4; i++) {
        if (filter_flags[i]) {
            p0 = samples[y-1][x];  p1 = samples[y-2][x];  p2 = samples[y-3][x];
            q0 = samples[y][x];    q1 = samples[y+1][x];  q2 = samples[y+2][x];

            aP = _abs(p2 - p0);
            aQ = _abs(q2 - q0);

            t = treshold + ((aP < beta) + (aQ < beta));


            delta = _clip3(-t, t,
                (((q0 - p0) * (1 << 2)) + (p1 - q1) + 4) >> 3);

            /*p1*/ samples[y-2][x] += (aP < beta) * _clip3(-treshold, treshold,
                  (p2 + ((p0 + q0 + 1) >> 1) - (p1 << 1)) >> 1);
            /*p0*/ samples[y-1][x] = _clip1y(p0 + delta, 8);

            /*q0*/ samples[y][x]   = _clip1y(q0 - delta, 8);
            /*q1*/ samples[y+1][x] += (aQ < beta) * _clip3(-treshold, treshold,
                  (q2 + ((p0 + q0 + 1) >> 1) - (q1 << 1)) >> 1);
        }

        x++;
    }
}

void filter_4p_vert_edge_high_bS_luma(int y, int x, const int filter_flags[4],
    uint8_t alpha, uint8_t beta, uint8_t samples[24][24]) {

    int aP, aQ;

    uint8_t p0, p1, p2, p3, q0, q1, q2, q3;

    for (int i = 0; i < 4; i++) {
        if (filter_flags[i]) {
            p0 = samples[y][x-1];  p1 = samples[y][x-2];  p2 = samples[y][x-3];  p3 = samples[y][x-4];
            q0 = samples[y][x];    q1 = samples[y][x+1];  q2 = samples[y][x+2];  q3 = samples[y][x+3];

            aP = _abs(p2 - p0);
            aQ = _abs(q2 - q0);

            if (aP < beta &&
                _abs(p0 - q0) < ((alpha >> 2) + 2)) {
                /*p0*/ samples[y][x-1] = (p2 + 2*p1 + 2*p0 + 2*q0 + q1 + 4) >> 3;
                /*p1*/ samples[y][x-2] = (p2 + p1 + p0 + q0 + 2) >> 2;
                /*p2*/ samples[y][x-3] = (2*p3 + 3*p2 + p1 + p0 + q0 + 4) >> 3;
            } else {
                /*p0*/ samples[y][x-1] = (2*p1 + p0 + q1 + 2) >> 2;
            }

            if (aQ < beta &&
                _abs(p0 - q0) < ((alpha >> 2) + 2)) {
                /*q0*/ samples[y][x]   = (p1 + 2*p0 + 2*q0 + 2*q1 + q2 + 4) >> 3;
                /*q1*/ samples[y][x+1] = (p0 + q0 + q1 + q2 + 2) >> 2;
                /*q2*/ samples[y][x+2] = (2*q3 + 3*q2 + q1 + q0 + p0 + 4) >> 3;
            } else {
                /*q0*/ samples[y][x]   = (2*q1 + q0 + p1 + 2) >> 2;
            }
        }

        y++;
    }
}

void filter_4p_hor_edge_high_bS_luma(int y, int x, const int filter_flags[4],
    uint8_t alpha, uint8_t beta, uint8_t samples[24][24]) {

    int aP, aQ;

    int p0, p1, p2, p3, q0, q1, q2, q3;

    for (int i = 0; i < 4; i++) {
        if (filter_flags[i]) {
            p0 = samples[y-1][x];  p1 = samples[y-2][x];  p2 = samples[y-3][x];  p3 = samples[y-4][x];
            q0 = samples[y][x];    q1 = samples[y+1][x];  q2 = samples[y+2][x];  q3 = samples[y+3][x];

            aP = _abs(p2 - p0);
            aQ = _abs(q2 - q0);

            if (aP < beta &&
                _abs(p0 - q0) < ((alpha >> 2) + 2)) {
                /*p0*/ samples[y-1][x] = (p2 + 2*p1 + 2*p0 + 2*q0 + q1 + 4) >> 3;
                /*p1*/ samples[y-2][x] = (p2 + p1 + p0 + q0 + 2) >> 2;
                /*p2*/ samples[y-3][x] = (2*p3 + 3*p2 + p1 + p0 + q0 + 4) >> 3;
            } else {
                /*p0*/ samples[y-1][x] = (2*p1 + p0 + q1 + 2) >> 2;
            }

            if (aQ < beta &&
                _abs(p0 - q0) < ((alpha >> 2) + 2)) {
                /*q0*/ samples[y][x]   = (p1 + 2*p0 + 2*q0 + 2*q1 + q2 + 4) >> 3;
                /*q1*/ samples[y+1][x] = (p0 + q0 + q1 + q2 + 2) >> 2;
                /*q2*/ samples[y+2][x] = (2*q3 + 3*q2 + q1 + q0 + p0 + 4) >> 3;
            } else {
                /*q0*/ samples[y][x]   = (2*q1 + q0 + p1 + 2) >> 2;
            }
        }

        x++;
    }
}

void filter_2p_vert_edge_low_bS_chroma(int y, int x, const int filter_flags[2],
    uint8_t bS, uint8_t indexA, uint8_t samples[16][16]) {

    int treshold = treshold_table[bS-1][indexA];
    int t, delta;

    int p0, p1, q0, q1;

    for (int i = 0; i < 2; i++) {
        if (filter_flags[i]) {
            p0 = samples[y][x-1];  p1 = samples[y][x-2];
            q0 = samples[y][x];    q1 = samples[y][x+1];

            t = treshold + 1;

            delta = _clip3(-t, t,
                (((q0 - p0) * (1 << 2)) + (p1 - q1) + 4) >> 3);

            /*p0*/ samples[y][x-1] = _clip1c(p0 + delta, 8);
            /*q0*/ samples[y][x]   = _clip1c(q0 - delta, 8);
        }

        y++;
    }
}

void filter_2p_hor_edge_low_bS_chroma(int y, int x, const int filter_flags[2],
    uint8_t bS, uint8_t indexA, uint8_t samples[16][16]) {

    int treshold = treshold_table[bS-1][indexA];
    int t, delta;

    int p0, p1, q0, q1;

    for (int i = 0; i < 2; i++) {
        if (filter_flags[i]) {
            p0 = samples[y-1][x];  p1 = samples[y-2][x];
            q0 = samples[y][x];    q1 = samples[y+1][x];

            t = treshold + 1;

            delta = _clip3(-t, t,
                (((q0 - p0) * (1 << 2)) + (p1 - q1) + 4) >> 3);

            /*p0*/ samples[y-1][x] = _clip1c(p0 + delta, 8);
            /*q0*/ samples[y][x]   = _clip1c(q0 - delta, 8);
        }

        x++;
    }
}

void filter_2p_vert_edge_high_bS_chroma(int y, int x, const int filter_flags[2], uint8_t samples[16][16]) {

    uint8_t p0, p1, q0, q1;

    for (int i = 0; i < 2; i++) {
        if (filter_flags[i]) {
            p0 = samples[y][x-1];  p1 = samples[y][x-2];
            q0 = samples[y][x];    q1 = samples[y][x+1];

            /*p0*/ samples[y][x-1] = (2*p1 + p0 + q1 + 2) >> 2;
            /*q0*/ samples[y][x]   = (2*q1 + q0 + p1 + 2) >> 2;
        }

        y++;
    }
}

void filter_2p_hor_edge_high_bS_chroma(int y, int x, const int filter_flags[2], uint8_t samples[16][16]) {

    int p0, p1, q0, q1;

    for (int i = 0; i < 2; i++) {
        if (filter_flags[i]) {
            p0 = samples[y-1][x];  p1 = samples[y-2][x];
            q0 = samples[y][x];    q1 = samples[y+1][x];

            /*p0*/ samples[y-1][x] = (2*p1 + p0 + q1 + 2) >> 2;
            /*q0*/ samples[y][x]   = (2*q1 + q0 + p1 + 2) >> 2;
        }

        x++;
    }
}


void filter_row_luma(Picture *pic, int mbAddr, int mbAddrN, uint8_t *dst, int y, uint8_t luma_block[24][24],
    uint8_t bS_list[4], int stride, Undo264Context *ctx) {

    uint8_t indexA, alpha, beta;
    int filter_flags[4];

    for (int i = 0; i < 4; i++) {
        derive_edge_treshold_luma(pic, mbAddr, mbAddrN, bS_list[i], 4+y, 4+i*4, false,
            &alpha, &beta, filter_flags, &indexA, luma_block, ctx);
    	if (bS_list[i] > 0) {
            if (bS_list[i] < 4) {
                filter_4p_hor_edge_low_bS_luma(4+y, 4+i*4, filter_flags, bS_list[i], indexA, beta, luma_block);
            } else {
                filter_4p_hor_edge_high_bS_luma(4+y, 4+i*4, filter_flags, alpha, beta, luma_block);
            }
        }
    }

    for (int k = 0; k < 16; k++) {
        for (int i = 0; i < 3; i++) {
            dst[i*stride]      = luma_block[y+4+i][k+4];   // q0, q1, q2
            dst[(-i-1)*stride] = luma_block[y+4-i-1][k+4]; // p0, p1, p2
        }
        dst++;
    }
}
void filter_col_luma(Picture *pic, int mbAddr, int mbAddrN, uint8_t *dst, int x, uint8_t luma_block[24][24],
    uint8_t bS_list[4], int stride, Undo264Context *ctx) {

    uint8_t indexA, alpha, beta;
    int filter_flags[4];


    for (int i = 0; i < 4; i++) {
        derive_edge_treshold_luma(pic, mbAddr, mbAddrN, bS_list[i], 4+i*4, 4+x, true,
            &alpha, &beta, filter_flags, &indexA, luma_block, ctx);
    	if (bS_list[i] > 0) {
            if (bS_list[i] < 4) {
                filter_4p_vert_edge_low_bS_luma(4+i*4, 4+x, filter_flags, bS_list[i], indexA, beta, luma_block);
            } else {
                filter_4p_vert_edge_high_bS_luma(4+i*4, 4+x, filter_flags, alpha, beta, luma_block);
            }
        }
    }

    for (int k = 0; k < 16; k++) {
        for (int i = 0; i < 3; i++) {
           dst[i]    = luma_block[k+4][x+4+i];   // q0, q1, q2
           dst[-i-1] = luma_block[k+4][x+4-i-1]; // p0, p1, p2
        }
        dst += stride;
    }
}

void filter_row_chroma(Picture *pic, int mbAddr, int mbAddrN, uint8_t *dst, int y, uint8_t samples[16][16],
    const uint8_t bS_list[4], int stride, Undo264Context *ctx) {

    uint8_t indexA, alpha, beta;
    int filter_flags[2];

    for (int i = 0; i < 4; i++) {
        int bS = bS_list[i];

        derive_edge_treshold_chroma(pic, mbAddr, mbAddrN, bS, 4+y, 4+i*2, false,
            &alpha, &beta, filter_flags, &indexA, samples, ctx);

        if (bS > 0) {
            if (bS < 4) {
                filter_2p_hor_edge_low_bS_chroma(4+y, 4+i*2, filter_flags, bS, indexA, samples);
            } else {
                filter_2p_hor_edge_high_bS_chroma(4+y, 4+i*2, filter_flags, samples);
            }
        }
    }

    for (int i = 0; i < 8; i++) {
        dst[0]       = samples[4+y][4+i];   // q0
        dst[-stride] = samples[4+y-1][4+i]; // p0
        dst++;
    }
}
void filter_col_chroma(Picture *pic, int mbAddr, int mbAddrN, uint8_t *dst, int x, uint8_t samples[16][16],
    const uint8_t bS_list[4], int stride, Undo264Context *ctx) {

    uint8_t indexA, alpha, beta;
    int filter_flags[2];

    for (int i = 0; i < 4; i++) {
        int bS = bS_list[i];

        derive_edge_treshold_chroma(pic, mbAddr, mbAddrN, bS, 4+i*2, 4+x, true,
            &alpha, &beta, filter_flags, &indexA, samples, ctx);

        if (bS > 0) {
            if (bS < 4) {
                filter_2p_vert_edge_low_bS_chroma(4+i*2, 4+x, filter_flags, bS, indexA, samples);
            } else {
                filter_2p_vert_edge_high_bS_chroma(4+i*2, 4+x, filter_flags, samples);
            }
        }
    }

    for (int i = 0; i < 8; i++) {
        dst[0]  = samples[i+4][4+x];   // q0
        dst[-1] = samples[i+4][4+x-1]; // p0
        dst += stride;
    }
}

void deblock_macroblock(Picture *pic, SliceHeader *sh, int mbAddr, Undo264Context *ctx) {
    SPS *sps = sh->sps;
    PPS *pps = sh->pps;



    // make dummy mb just for accessing the neighbors afterward
    Macroblock *mb = make_mb(mbAddr, ctx);
    derive_macroblock_neighbors(mb, sh->first_mb, ctx);


    const int widthY     = pic->widthY;
    const int widthC     = pic->widthC;

    const bool disableSliceBoundaries = sh->disable_deblocking_filter_idc == 2;
    const bool filterInternalEdges    = sh->disable_deblocking_filter_idc != 1;
    const bool filterLeftMbEdge       = filterInternalEdges && (mb->mbAddr % sps->pic_width_in_mbs != 0) && (!disableSliceBoundaries || mb->has_mb_a);
    const bool filterTopMbEdge        = filterInternalEdges && (mb->mbAddr >= sps->pic_width_in_mbs) && (!disableSliceBoundaries || mb->has_mb_b);


    const int luma_pos   = mb->mb_y*16*widthY + mb->mb_x*16;
    const int chroma_pos = mb->mb_y*8*widthC + mb->mb_x*8;
    uint8_t *luma_base_dst = &pic->luma[luma_pos];
    uint8_t *cb_base_dst   = &pic->cb[chroma_pos];
    uint8_t *cr_base_dst   = &pic->cr[chroma_pos];


    uint8_t luma_block[24][24], cb_block[16][16], cr_block[16][16];
    fetch_24x24_luma_block(luma_block, luma_pos, widthY, pic);
    fetch_16x16_chroma_block(cb_block, cr_block, chroma_pos, widthC, pic);

    uint8_t bS_list[4];

    bool mb8x8 = ctx->mb_metadata[mb->mbAddr].t_8x8_flag;

    if (filterLeftMbEdge) {
        // x = 0
        derive_edge_bS_list(mbAddr, mbAddr + mb->mb_a_off,
            0, 3, 0, 1,
            true, true, bS_list, ctx);

        filter_col_luma(pic, mbAddr, mbAddr + mb->mb_a_off, luma_base_dst, 0, luma_block, bS_list, widthY, ctx);
        filter_col_chroma(pic, mbAddr, mbAddr + mb->mb_a_off, cb_base_dst, 0, cb_block, bS_list, widthC, ctx);
        filter_col_chroma(pic, mbAddr, mbAddr + mb->mb_a_off, cr_base_dst, 0, cr_block, bS_list, widthC, ctx);
    }
    if (filterInternalEdges) {

        // x = 4
        if (!mb8x8) {
            derive_edge_bS_list(mbAddr, mbAddr,
                1, 0, 0, 0,
                false, true, bS_list, ctx);
            filter_col_luma(pic, mbAddr, mbAddr, luma_base_dst +  4,  4, luma_block, bS_list, widthY, ctx);
        }


        // x = 8
        derive_edge_bS_list(mbAddr, mbAddr,
            2, 1, 1, 0,
            false, true, bS_list, ctx);

        filter_col_luma(pic, mbAddr, mbAddr, luma_base_dst + 8, 8, luma_block, bS_list, widthY, ctx);
        filter_col_chroma(pic, mbAddr, mbAddr, cb_base_dst + 4, 4, cb_block, bS_list, widthC, ctx);
        filter_col_chroma(pic, mbAddr, mbAddr, cr_base_dst + 4, 4, cr_block, bS_list, widthC, ctx);


        // x = 12
        if (!mb8x8) {
            derive_edge_bS_list(mbAddr, mbAddr,
                3, 2, 1, 1,
                false, true, bS_list, ctx);
            filter_col_luma(pic, mbAddr, mbAddr, luma_base_dst + 12, 12, luma_block, bS_list, widthY, ctx);
        }
    }


    if (filterTopMbEdge) {
        // y = 0
        derive_edge_bS_list(mbAddr, mbAddr + mb->mb_b_off,
            0, 12, 0, 2,
            true, false, bS_list, ctx);


        filter_row_luma(pic, mbAddr, mbAddr + mb->mb_b_off, luma_base_dst, 0, luma_block, bS_list, widthY, ctx);
        filter_row_chroma(pic, mbAddr, mbAddr + mb->mb_b_off, cb_base_dst, 0, cb_block, bS_list, widthC, ctx);
        filter_row_chroma(pic, mbAddr, mbAddr + mb->mb_b_off, cr_base_dst, 0, cr_block, bS_list, widthC, ctx);
    }
    if (filterInternalEdges) {

        // y = 4
        if (!mb8x8) {
            derive_edge_bS_list(mbAddr, mbAddr,
                4, 0, 0, 0,
                false, false, bS_list, ctx);
            filter_row_luma(pic, mbAddr, mbAddr, luma_base_dst +  4*widthY, 4, luma_block, bS_list, widthY, ctx);
        }


        // y = 8
        derive_edge_bS_list(mbAddr, mbAddr,
            8, 4, 2, 0,
            false, false, bS_list, ctx);

        filter_row_luma(pic, mbAddr, mbAddr, luma_base_dst + 8*widthY, 8, luma_block, bS_list, widthY, ctx);
        filter_row_chroma(pic, mbAddr, mbAddr, cb_base_dst + 4*widthC, 4, cb_block, bS_list, widthC, ctx);
        filter_row_chroma(pic, mbAddr, mbAddr, cr_base_dst + 4*widthC, 4, cr_block, bS_list, widthC, ctx);


        // y = 12
        if (!mb8x8) {
            derive_edge_bS_list(mbAddr, mbAddr,
                12, 8, 2, 2,
                false, false, bS_list, ctx);
            filter_row_luma(pic, mbAddr, mbAddr, luma_base_dst + 12*widthY, 12, luma_block, bS_list, widthY, ctx);
        }
    }

    free(mb);
}


void deblock_slice(Picture *pic, SliceHeader *sh, Undo264Context *ctx) {
    for (int i = sh->first_mb; i < sh->first_mb + ctx->current_slice->num_mbs; i++) {
        deblock_macroblock(pic, sh, i, ctx);
    }
}