//
// Created by gmathix on 5/12/26.
//

#include "mb.h"
#include "picture.h"



static const uint8_t alpha_table[52] = {
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   4,   4,   5,   6,   7,   8,   9,  10,  12,  13,
     15,  17,  20,  22,  25,  28,  32,  36,  40,  45,  50,  56,  63,
     71,  80,  90, 101, 113, 127, 144, 162, 182, 203, 226, 255, 255,
};

static const uint8_t beta_table[52] = {
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  2,  2,  2,  3,  3,  3,  3,  4,  4,  4,
     6,  6,  7,  7,  8,  8,  9,  9, 10, 10, 11, 11, 12,
    12, 13, 13, 14, 14, 15, 15, 16, 16, 17, 17, 18, 18,
};

static const uint8_t treshold_table[3][52] = {
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


static ALWAYS_INLINE void fetch_24x24_luma_block(uint8_t block[24][24], int pos, int stride, Picture *pic) {
    int pos_clip;
    for (int y = 0; y < 24; y++) {
        for (int x = 0; x < 24; x++) {
            pos_clip = _clip3(0, pic->width*pic->height - 1, pos + (y-4)*stride + x-4);
            block[y][x] = pic->luma[pos_clip];
        }
    }
}
static ALWAYS_INLINE void fetch_16x16_chroma_block(uint8_t cb_block[16][16], uint8_t cr_block[16][16], int pos, int stride, Picture *pic) {
    int pos_clip;
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            pos_clip = _clip3(0, pic->width/2 *pic->height/2 - 1, pos + (y-4)*stride + x-4);
            cb_block[y][x] = pic->cb[pos_clip];
            cr_block[y][x] = pic->cr[pos_clip];
        }
    }
}


/**
 * derive the 4 bS values needed for a vertical or horizontal edge
 * @param mb macroblock containing the sample q0
 * @param mbAddrN address of the macroblock containing the sample p0
 * @param mb_4x4_idx initial 4x4 block index in mb
 * @param mb_n_4x4_idx initial 4x4 block index in mb_n
 * @param mb_8x8_idx initial 8x8 block_index in mb
 * @param mb_n_8x8_idx initial 8x8 block index in mb_n
 */
static ALWAYS_INLINE void derive_edge_bS_list(Macroblock *mb, int mbAddrN,
                                            int mb_4x4_idx, int mb_n_4x4_idx, int mb_8x8_idx, int mb_n_8x8_idx,
                                            bool mb_edge, bool vertical, uint8_t bS_list[4], CodecContext *ctx) {

    int blkAdd = 1 + vertical*3; // 1 for horizontal, 4 for vertical
    int blkAdd8x8 = 1 + vertical;
    int curr_bS = 0;

    for (int i = 0; i < 4; i++) {
        int idx       = mb_4x4_idx + i*blkAdd;
        int idx_n     = mb_n_4x4_idx + i*blkAdd;
        int idx_8x8   = mb_8x8_idx + (i/2)*blkAdd8x8;
        int idx_n_8x8 = mb_n_8x8_idx + (i/2)*blkAdd8x8;

        MotionVector mv1 = ctx->mvs_l0[mb->mbAddr][idx];
        MotionVector mv2 = ctx->mvs_l0[mbAddrN][idx_n];

        /*
         * bS = 4
         * <=> edge is a macroblock edge and either or both of mb and mb_n are intra macroblocks
         */
        curr_bS += (mb_edge && (IS_INTRA(mb->mb_type) || IS_INTRA(ctx->mb_types[mbAddrN]))) * 4;

        /*
         * bS = 3
         * <=> either or both of mb and mb_n are intra macroblocks
         */
        curr_bS += ((curr_bS == 0) && (IS_INTRA(mb->mb_type) || IS_INTRA(ctx->mb_types[mbAddrN]))) * 3;

        /*
         * bS = 2
         * <=> the corresponding 4x4 or 8x8 transform blocks (depending on transform_8x8_flag) have non-zero coeff levels
         */
        curr_bS += ((curr_bS == 0) && /*((mb->t_8x8_flag    * (1 & (mb->residuals.cbp_luma << idx_8x8))) || */
                                 // (mb_n->t_8x8_flag  * (1 & (mb_n->residuals.cbp_luma << idx_n_8x8))) +
                                 (!mb->t_8x8_flag   * (ctx->luma_total_coeffs[mb->mbAddr][idx] > 0)) ||
                                 ((ctx->luma_total_coeffs[mbAddrN][idx_n] > 0))) * 2;

        /* bS = 1 (simplified for now)
         * <=> - the respective macroblock partitions containing p0 and q0 use different reference pictures
         *       or a different number or MVs
         *     - both use the same MV and | mv1.x - mv2.x | >= 4
         *                             or | mv1.y - mv2.y | >= 4
         */
        curr_bS += ((curr_bS == 0) && ((mv1.ref_idx != mv2.ref_idx) ||
                                 (ctx->pred_flag_l0[mb->mbAddr][idx_8x8] != ctx->pred_flag_l0[mbAddrN][idx_n_8x8]) ||
                                 ((_abs(mv1.x - mv2.x) >= 4) + (_abs(mv1.y - mv2.y) >= 4))));

        bS_list[i] = curr_bS;
        curr_bS = 0;
    }
}


static ALWAYS_INLINE void derive_edge_treshold(Macroblock *mb, int mbAddrN, uint8_t bS_list[4], uint8_t samples[24][24], int blk_idx, bool vertical,
    uint8_t *alpha, uint8_t *beta, uint8_t filter_flags[4], uint8_t *indexA, CodecContext *ctx) {

    SPS *sps = mb->p_pic->sh->sps;

    int qpAv = (mb->QPY + ctx->QPs[mbAddrN] + 1) >> 1;

    *indexA    = _clip3(0, 51, qpAv + mb->p_pic->sh->slice_alpha_c0_offset_div2 * 2);
    int indexB = _clip3(0, 51, qpAv + mb->p_pic->sh->slice_beta_offset_div2 * 2);

    *alpha = alpha_table[*indexA]; // assume bitDepth = 8
    *beta  = beta_table[indexB];


    int blkY = (blk_idx >> 2) + 4;
    int blkX = (blk_idx  & 3) + 4;

    if (vertical) {
        for (int i = 0; i < 4; i++) {
            filter_flags[i] = (bS_list[i] != 0) &&
                (_abs(samples[blkY][blkX-1] - samples[blkY][blkX]) < *alpha) &&
                (_abs(samples[blkY][blkX-2] - samples[blkY][blkX-1]) < *beta) &&
                (_abs(samples[blkY][blkX+1] - samples[blkY][blkX] < *beta));
            blkY++;
        }
    } else {
        for (int i = 0; i < 4; i++) {
            filter_flags[i] = (bS_list[i] != 0) &&
                (_abs(samples[blkY-1][blkX] - samples[blkY][blkX]) < *alpha) &&
                (_abs(samples[blkY-2][blkX] - samples[blkY-1][blkX]) < *beta) &&
                (_abs(samples[blkY+1][blkX] - samples[blkY][blkX] < *beta));
            blkX++;
        }
    }
}


static ALWAYS_INLINE void filter_4p_vert_edge_low_bS(uint8_t y, uint8_t x, uint8_t s[24][24], uint8_t filter_flags[4], uint8_t bS, uint8_t indexA, uint8_t beta, bool luma) {
    int treshold = treshold_table[bS-1][indexA];
    int aP, aQ, t, delta;

    int p0, p1, p2, p3, q0, q1, q2, q3;

    for (int i = 0; i < 4; i++) {
        if (filter_flags[i]) {
            p0 = s[y][x-1];  p1 = s[y][x-2];  p2 = s[y][x-3];  p3 = s[y][x-4];
            q0 = s[y][x];    q1 = s[y][x+1];  q2 = s[y][x+2];  q3 = s[y][x+3];

            aP = _abs(p2 - p0);
            aQ = _abs(q2 - q0);

            t = luma
                ? treshold + ((aP < beta) + (aQ < beta))
                : treshold + 1;

            delta = _clip3(-t, t,
                (((q0 - p0) << 2) + (p1 - q1) + 4) >> 3);

            s[y][x-1] = _clip1y(p0 + delta, 8);
            s[y][x]   = _clip1y(q0 - delta, 8);
            s[y][x-2] += luma * (aP < beta) * _clip3(-t, t,
                (p2 + ((p0 + q0 + 1) >> 1) - (p1 << 1)) >> 1);
            s[y][x+1] += luma * (aQ < beta) * _clip3(-t, t,
                (q2 + ((p0 + q0 + 1) >> 1) - (q1 << 1)) >> 1);
        }

        y++;
    }
}

static ALWAYS_INLINE void filter_4p_hor_edge_low_bS(uint8_t y, uint8_t x, uint8_t s[24][24], uint8_t filter_flags[4], uint8_t bS, uint8_t indexA, uint8_t beta, bool luma) {
    int treshold = treshold_table[bS-1][indexA];
    int aP, aQ, t, delta;

    int p0, p1, p2, p3, q0, q1, q2, q3;

    for (int i = 0; i < 4; i++) {
        if (filter_flags[i]) {
            p0 = s[y-1][x];  p1 = s[y-2][x];  p2 = s[y-3][x];  p3 = s[y-4][x];
            q0 = s[y][x];    q1 = s[y+1][x];  q2 = s[y+2][x];  q3 = s[y+3][x];

            aP = _abs(p2 - p0);
            aQ = _abs(q2 - q0);

            t = luma
                ? treshold + ((aP < beta) + (aQ < beta))
                : treshold + 1;

            delta = _clip3(-t, t,
                (((q0 - p0) << 2) + (p1 - q1) + 4) >> 3);

            s[y-1][x] = _clip1y(p0 + delta, 8);
            s[y][x]   = _clip1y(q0 - delta, 8);
            s[y-2][x] += luma * (aP < beta) * _clip3(-t, t,
                (p2 + ((p0 + q0 + 1) >> 1) - (p1 << 1)) >> 1);
            s[y+1][x] += luma * (aQ < beta) * _clip3(-t, t,
                (q2 + ((p0 + q0 + 1) >> 1) - (q1 << 1)) >> 1);

            x++;
        }
    }
}

static ALWAYS_INLINE void filter_4p_vert_edge_high_bS(uint8_t y, uint8_t x, uint8_t s[24][24], uint8_t filter_flags[4], uint8_t alpha, uint8_t beta, bool luma) {
    int aP, aQ, t, delta;

    uint8_t p0, p1, p2, p3, q0, q1, q2, q3;

    for (int i = 0; i < 4; i++) {
        if (filter_flags[i]) {
            p0 = s[y][x-1];  p1 = s[y][x-2];  p2 = s[y][x-3];  p3 = s[y][x-4];
            q0 = s[y][x];    q1 = s[y][x+1];  q2 = s[y][x+2];  q3 = s[y][x+3];

            aP = _abs(p2 - p0);
            aQ = _abs(q2 - q0);

            if (luma && aP < beta &&
                _abs(p0 - q0) < ((alpha >> 2) + 2)) {
                s[y][x-1] = (p2 + 2*p1 + 2*p0 + 2*q0 + q1 + 4) >> 3;
                s[y][x-2] = (p2 + p1 + p0 + q0 + 2) >> 2;
                s[y][x-3] = (2*p3 + 3*p2 + p1 + p0 + q0 + 4) >> 3;
            } else {
                s[y][x-1] = (2*p1 + p0 + q1 + 2) >> 2;
                s[y][x-2] = p1;
                s[y][x-3] = p2;
            }

            if (luma && aQ < beta &&
                _abs(p0 - q0) < ((alpha >> 2) + 2)) {
                s[y][x]   = (p1 + 2*p0 + 2*q0 + 2*q1 + q2 + 4) >> 3;
                s[y][x+1] = (p0 + q0 + q1 + q2 + 2) >> 2;
                s[y][x+2] = (2*q3 + 3*q2 + q1 + q0 + p0 + 4) >> 3;
            } else {
                s[y][x]   = (2*q1 + q0 + p1 + 2) >> 2;
                s[y][x+1] = q1;
                s[y][x+2] = q2;
            }
        }

        y++;
    }
}

static ALWAYS_INLINE void filter_4p_hor_edge_high_bS(uint8_t y, uint8_t x, uint8_t s[24][24], uint8_t filter_flags[4], uint8_t alpha, uint8_t beta, bool luma) {
    int aP, aQ, t, delta;

    int p0, p1, p2, p3, q0, q1, q2, q3;

    for (int i = 0; i < 4; i++) {
        if (filter_flags[i]) {
            p0 = s[y-1][x];  p1 = s[y-2][x];  p2 = s[y-3][x];  p3 = s[y-4][x];
            q0 = s[y][x];    q1 = s[y+1][x];  q2 = s[y+2][x];  q3 = s[y+3][x];

            aP = _abs(p2 - p0);
            aQ = _abs(q2 - q0);

            if (luma && aP < beta &&
                _abs(p0 - q0) < ((alpha >> 2) + 2)) {
                s[y-1][x] = (p2 + 2*p1 + 2*p0 + 2*q0 + q1 + 4) >> 3;
                s[y-2][x] = (p2 + p1 + p0 + q0 + 2) >> 2;
                s[y-3][x] = (2*p3 + 3*p2 + p1 + p0 + q0 + 4) >> 3;
                } else {
                    s[y-1][x] = (2*p1 + p0 + q1 + 2) >> 2;
                    s[y-2][x] = p1;
                    s[y-3][x] = p2;
                }

            if (luma && aQ < beta &&
                _abs(p0 - q0) < ((alpha >> 2) + 2)) {
                s[y][x]   = (p1 + 2*p0 + 2*q0 + 2*q1 + q2 + 4) >> 3;
                s[y+1][x] = (p0 + q0 + q1 + q2 + 2) >> 2;
                s[y+2][x] = (2*q3 + 3*q2 + q1 + q0 + p0 + 4) >> 3;
                } else {
                    s[y][x]   = (2*q1 + q0 + p1 + 2) >> 2;
                    s[y+1][x] = q1;
                    s[y+2][x] = q2;
                }
        }

        x++;
    }
}


static ALWAYS_INLINE void filter_row_luma(Macroblock *mb, int mbAddrN, uint8_t *dst, int blkIdx, int y, uint8_t luma_block[24][24],
    uint8_t bS_list[4], int stride, CodecContext *ctx) {

    uint8_t indexA, alpha, beta;
    uint8_t filter_flags[4];

    for (int i = 0; i < 4; i++) {
        derive_edge_treshold(mb, mbAddrN, bS_list, luma_block, (y/4)*4+i, false, &alpha, &beta, filter_flags, &indexA, ctx);
        if (bS_list[i] > 0) {
            if (bS_list[i] < 4) {
                filter_4p_hor_edge_low_bS(4+y, 4+i*4, luma_block, filter_flags, bS_list[i], indexA, beta, true);
            } else {
                filter_4p_hor_edge_high_bS(4+y, 4+i*4, luma_block, filter_flags, alpha, beta, true);
            }
        }
    }

    for (int k = 0; k < 16; k++) {
        for (int i = 0; i < 4; i++) {
            *(dst + i*stride)     = luma_block[y+4+i][k+4];
            *(dst - i*stride - stride) = luma_block[y+4-i-1][k+4];
        }
        dst++;
    }
}
static ALWAYS_INLINE void filter_col_luma(Macroblock *mb, int mbAddrN, uint8_t *dst, int x, uint8_t luma_block[24][24],
    uint8_t bS_list[4], int stride, CodecContext *ctx) {

    uint8_t indexA, alpha, beta;
    uint8_t filter_flags[4];

    for (int i = 0; i < 4; i++) {
        derive_edge_treshold(mb, mbAddrN, bS_list, luma_block, i*4+x/4, true, &alpha, &beta, filter_flags, &indexA, ctx);
        if (bS_list[i] > 0) {
            if (bS_list[i] < 4) {
                filter_4p_vert_edge_low_bS(4+i*4, 4+x, luma_block, filter_flags, bS_list[i], indexA, beta, true);
            } else {
                filter_4p_vert_edge_high_bS(4+i*4, 4+x, luma_block, filter_flags, alpha, beta, true);
            }
        }
    }

    for (int k = 0; k < 16; k++) {
        for (int i = 0; i < 4; i++) {
            *(dst+i)   = luma_block[k+4][x+4+i];
            *(dst-i-1) = luma_block[k+4][x+4-i-1];
        }
        dst += stride;
    }
}

static ALWAYS_INLINE void filter_row_chroma(Macroblock *mb, uint8_t *dst, int y, uint8_t chroma_block[16][16],
    uint8_t bS_list[4], int stride) {

    for (int k = 0; k < 8; k++) {


        for (int i = 0; i < 4; i++) {
            *(dst + i*stride)     = chroma_block[y+4+i][k+4];
            *(dst - i*stride - stride) = chroma_block[y+4-i-1][k+4];
        }

        dst++;
    }
}
static ALWAYS_INLINE void filter_col_chroma(Macroblock *mb, uint8_t *dst, int x, uint8_t chroma_block[16][16],
    uint8_t bS_list[4], int stride) {

    for (int k = 0; k < 8; k++) {


        for (int i = 0; i < 4; i++) {
            *(dst+i)   = chroma_block[k+4][x+4+i];
            *(dst-i-1) = chroma_block[k+4][x+4-i-1];
        }

        dst += stride;
    }
}


static ALWAYS_INLINE void deblock_inloop(Macroblock *mb, CodecContext *ctx) {
    Picture *pic = mb->p_pic;
    SliceHeader *sh = pic->sh;
    SPS *sps = sh->sps;
    PPS *pps = sh->pps;
    


    bool fieldMbInFrame      = 0;
    bool filterInternalEdges = !sh->disable_deblocking_filter_idc;
    bool filterLeftMbEdge    = mb->has_mb_a && filterInternalEdges;
    bool filterTopMbEdge     = mb->has_mb_b && filterInternalEdges;

    int strideY    = pic->strideY;
    int strideC    = pic->strideC;
    int luma_pos   = mb->mb_y*16*strideY + mb->mb_x*16;
    int chroma_pos = mb->mb_y*8*strideC + mb->mb_x*8;
    uint8_t *luma_base_dst = &pic->luma[luma_pos];
    uint8_t *cb_base_dst   = &pic->cb[chroma_pos];
    uint8_t *cr_base_dst   = &pic->cr[chroma_pos];

    int topEdgeBs  = 0;
    int leftEdgeBs = 0;
    int intraBs    = 0;

    uint8_t luma_block[24][24], cb_block[16][16], cr_block[16][16];
    fetch_24x24_luma_block(luma_block, luma_pos, strideY, pic);
    fetch_16x16_chroma_block(cb_block, cr_block, chroma_pos, strideC, pic);

    uint8_t bS_list[4];
    uint8_t filter_flags[4];
    uint8_t alpha, beta, indexA;

    // printf("mbAddr :%d x:%d y:%d mb_a_off:%d\n", mb->mbAddr, mb->mb_x, mb->mb_y, mb->mb_a_off);
    if (filterLeftMbEdge) {
        derive_edge_bS_list(mb, mb->mbAddr + mb->mb_a_off,
            0, 3, 0, 1,
            true, true, bS_list, ctx);

        filter_col_luma(mb, mb->mbAddr + mb->mb_a_off, luma_base_dst, 0, luma_block, bS_list, strideY, ctx);
        filter_col_chroma(mb, cb_base_dst, 0, cb_block, bS_list, strideC);
        filter_col_chroma(mb, cr_base_dst, 0, cr_block, bS_list, strideC);
    }
    if (filterInternalEdges) {
        derive_edge_bS_list(mb, mb->mbAddr,
            2, 1, 1, 0,
            false, true, bS_list, ctx);

        filter_col_luma(mb, mb->mbAddr, luma_base_dst + 8, 8, luma_block, bS_list, strideY, ctx);
        filter_col_chroma(mb, cb_base_dst + 4, 4, cb_block, bS_list, strideC);
        filter_col_chroma(mb, cr_base_dst + 4, 4, cr_block, bS_list, strideC);

        if (!mb->t_8x8_flag) {
            derive_edge_bS_list(mb, mb->mbAddr,
                1, 0, 0, 0,
                false, true, bS_list, ctx);
            filter_col_luma(mb, mb->mbAddr, luma_base_dst +  4,  4, luma_block, bS_list, strideY, ctx);

            derive_edge_bS_list(mb, mb->mbAddr,
                3, 2, 1, 1,
                false, true, bS_list, ctx);
            filter_col_luma(mb, mb->mbAddr, luma_base_dst + 12, 12, luma_block, bS_list, strideY, ctx);
        }
    }

    if (filterTopMbEdge) {
        derive_edge_bS_list(mb, mb->mbAddr + mb->mb_b_off,
            0, 12, 0, 2,
            true, false, bS_list, ctx);


        filter_row_luma(mb, mb->mbAddr + mb->mb_b_off, luma_base_dst, 0, 0, luma_block, bS_list, strideY, ctx);
        filter_col_chroma(mb, cb_base_dst, 0, cb_block, bS_list, strideC);
        filter_col_chroma(mb, cr_base_dst, 0, cr_block, bS_list, strideC);
    }
    if (filterInternalEdges) {
        derive_edge_bS_list(mb, mb->mbAddr,
            8, 4, 2, 0,
            false, false, bS_list, ctx);

        filter_row_luma(mb, mb->mbAddr, luma_base_dst + 8*strideY, 8, 8, luma_block, bS_list, strideY, ctx);
        filter_row_chroma(mb, cb_base_dst + 4*strideC, 4, cb_block, bS_list, strideC);
        filter_row_chroma(mb, cr_base_dst+ 4*strideC, 4, cr_block, bS_list, strideC);

        if (!mb->t_8x8_flag) {
            derive_edge_bS_list(mb, mb->mbAddr,
                4, 0, 0, 0,
                false, false, bS_list, ctx);
            filter_row_luma(mb, mb->mbAddr, luma_base_dst +  4*strideY, 4, 4, luma_block, bS_list, strideY, ctx);

            derive_edge_bS_list(mb, mb->mbAddr,
                12, 8, 2, 2,
                false, false, bS_list, ctx);
            filter_row_luma(mb, mb->mbAddr, luma_base_dst + 12*strideY, 12, 12, luma_block, bS_list, strideY, ctx);
        }
    }
}