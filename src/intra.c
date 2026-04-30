//
// Created by gmathix on 4/6/26.
//

#include "intra.h"

#include <string.h>

#include "picture.h"
#include "util/mbutil.h"



static const intra_pred_4x4_func intra4x4_table[9] = {
    vert_4x4_pred,
    hor_4x4_pred,
    dc_4x4_pred,
    diag_down_left_4x4_pred,
    diag_down_right_4x4_pred,
    vert_right_4x4_pred,
    hor_down_4x4_pred,
    vert_left_4x4_pred,
    hor_up_4x4_pred
};

static const intra_pred_16x16_func intra16x16_table[4] = {
    vert_16x16_pred,
    hor_16x16_pred,
    dc_16x16_pred,
    plane_16x16_pred
};

static const intra_pred_8x8_chroma_func intra8x8_chroma_table[4] = {
    dc_8x8_chroma_pred,
    hor_8x8_chroma_pred,
    vert_8x8_chroma_pred,
    plane_8x8_chroma_pred
};




/*=======================================*/
/*========   4x4 PREDICTION   ===========*/
/*=======================================*/

void vert_4x4_pred(uint8_t *dst, int stride,  int a_av, int b_av,
    const uint8_t top_samples[9], const uint8_t left_samples[5]) {

    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            *dst++ = top_samples[1+x];
        }
        dst += stride - 4;
    }
}

void hor_4x4_pred(uint8_t *dst, int stride,  int a_av, int b_av,
    const uint8_t top_samples[9], const uint8_t left_samples[5]) {

    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            *dst++ = left_samples[1+y];
        }
        dst += stride - 4;
    }
}

void dc_4x4_pred(uint8_t *dst, int stride,  int a_av, int b_av,
    const uint8_t top_samples[9], const uint8_t left_samples[5]) {

    int dc;
    if (a_av && b_av) {
        dc = (top_samples[1] + top_samples[2] + top_samples[3] + top_samples[4] +
              left_samples[1] + left_samples[2] + left_samples[3] + left_samples[4] + 4) >> 3;
    } else if (a_av && !b_av) {
        dc = (left_samples[1] + left_samples[2] + left_samples[3] + left_samples[4] + 2) >> 2;
    } else if (!a_av && b_av) {
        dc = (top_samples[1] + top_samples[2] + top_samples[3] + top_samples[4] + 2) >> 2;
    } else {
        dc = 1 << (8-1); // just assume BitDepthY = 8 because i don't want to add CodecContext everywhere here
    }
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            *dst++ = (int16_t)dc;
        }
        dst += stride - 4;
    }
}

void diag_down_left_4x4_pred(uint8_t *dst, int stride,  int a_av, int b_av,
    const uint8_t top_samples[9], const uint8_t left_samples[5]) {

    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            if (y == 3 && x == 3)
                *dst++ = (top_samples[7] + 3*top_samples[8] + 2) >> 2;
            else
                *dst++ = (top_samples[1+x+y] + 2*top_samples[1+x+y+1] + top_samples[1+x+y+2] + 2) >> 2;
        }
        dst += stride - 4;
    }
}

void diag_down_right_4x4_pred(uint8_t *dst, int stride,  int a_av, int b_av,
    const uint8_t top_samples[9], const uint8_t left_samples[5]) {

    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            if      (x == y)   *dst++ = (top_samples[1] + 2*top_samples[0] + left_samples[1] + 2) >> 2;
            else if (x > y)    *dst++ = (top_samples[1+x-y-2] + 2*top_samples[1+x-y-1] + top_samples[1+x-y] + 2) >> 2;
            else               *dst++ = (left_samples[1+y-x-2] + 2*left_samples[1+y-x-1] + left_samples[1+y-x] + 2) >> 2;
        }
        dst += stride - 4;
    }
}

void vert_right_4x4_pred(uint8_t *dst, int stride,  int a_av, int b_av,
    const uint8_t top_samples[9], const uint8_t left_samples[5]) {

    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            int zVR = 2*x-y;
            if (zVR >= 0) {
                if (!(zVR&1)) {
                    *dst++ =
                        (top_samples[x-(y>>1)]+top_samples[1+x-(y>>1)] + 1) >> 1;
                } else if (zVR&1) {
                    *dst++ =
                        (top_samples[x-(y>>1)-1] + 2*top_samples[x-(y>>1)] + top_samples[1+x-(y>>1)] + 2) >> 2;
                }
            }
            else if (zVR == -1) {
                *dst++ =
                    (left_samples[1] + 2*left_samples[0] + top_samples[1] + 2) >> 2;
            } else {
                *dst++ =
                    (left_samples[y] + 2*left_samples[y-1] + left_samples[y-2] + 2) >> 2;
            }
        }
        dst += stride - 4;
    }
}

void hor_down_4x4_pred(uint8_t *dst, int stride,  int a_av, int b_av,
    const uint8_t top_samples[9], const uint8_t left_samples[5]) {

    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            int zHD = 2*y-x;
            if (zHD >= 0) {
                if (!(zHD&1)) {
                    *dst++ =
                        (left_samples[y-(x>>1)] + left_samples[1+y-(x>>1)] + 1) >> 1;
                } else if (zHD&1) {
                    *dst++ =
                        (left_samples[y-(x>>1)-1] + 2*left_samples[y-(x>>1)] + left_samples[1+y-(x>>1)] + 2) >> 2;
                }
            }
            else if (zHD == -1) {
                *dst++ =
                    (left_samples[1] + 2*left_samples[0] + top_samples[1] + 2) >> 2;
            } else {
                *dst++ =
                    (top_samples[x] + 2*top_samples[x-1] + top_samples[x-2] + 2) >> 2;
            }
        }
        dst += stride - 4;
    }
}

void vert_left_4x4_pred(uint8_t *dst, int stride,  int a_av, int b_av,
    const uint8_t top_samples[9], const uint8_t left_samples[5]) {

    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            if (y&1) {
                *dst++ = (top_samples[1+x+(y>>1)] + 2*top_samples[2+x+(y>>1)] + top_samples[3+x+(y>>1)] + 2) >> 2;
            } else {
                *dst++ = (top_samples[1+x+(y>>1)] + top_samples[2+x+(y>>1)] + 1) >> 1;
            }
        }
        dst += stride - 4;
    }
}

void hor_up_4x4_pred(uint8_t *dst, int stride,  int a_av, int b_av,
    const uint8_t top_samples[9], const uint8_t left_samples[5]) {

    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            int zHU = x+2*y;
            if (zHU < 5) {
                if (!(zHU&1)) {
                    *dst++ =
                        (left_samples[1+y+(x>>1)] + left_samples[2+y+(x>>1)] + 1) >> 1;
                } else if (zHU&1) {
                    *dst++ =
                        (left_samples[1+y+(x>>1)] + 2*left_samples[2+y+(x>>1)] + left_samples[3+y+(x>>1)] + 2) >> 2;
                }
            } else if (zHU == 5) {
                *dst++ =
                    (left_samples[3] + 3*left_samples[4] + 2) >> 2;
            } else {
                *dst++ = left_samples[4];
            }
        }
        dst += stride - 4;
    }
}





/*=========================================*/
/*========   16x16 PREDICTION   ===========*/
/*=========================================*/

void vert_16x16_pred(uint8_t *dst, int stride, int a_av, int b_av,
    const uint8_t top_samples[17], const uint8_t left_samples[17], int bitDepth) {

    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 16; j++) {
            *dst++ = top_samples[j+1];
        }
        dst += stride - 16;
    }
}


void hor_16x16_pred(uint8_t *dst, int stride, int a_av, int b_av,
    const uint8_t top_samples[17], const uint8_t left_samples[17], int bitDepth) {

    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            *dst++ = left_samples[y+1];
        }
        dst += stride - 16;
    }
}

void dc_16x16_pred(uint8_t *dst, int stride, int a_av, int b_av,
    const uint8_t top_samples[17], const uint8_t left_samples[17], int bitDepth) {

    if (!a_av && !b_av) {
        for (int y = 0; y < 16; y++) {
            for (int x = 0; x < 16; x++) {
                *dst++ = 1 << (bitDepth-1);
            }
            dst += stride - 16;
        }
    } else {
        int sum = 0;
        int32_t dc;

        if (a_av && b_av) {
            for (int i = 0; i < 16; i++) sum += top_samples[i+1];
            for (int i = 0; i < 16; i++) sum += left_samples[i+1];
            dc = (sum + 16) >> 5;
        } else if (a_av) {
            for (int i = 0; i < 16; i++) sum += left_samples[i+1];
            dc = (sum + 8) >> 4;
        } else {
            for (int i = 0; i < 16; i++) sum += top_samples[i+1];
            dc = (sum + 8) >> 4;
        }

        for (int y = 0; y < 16; y++) {
            for (int x = 0; x < 16; x++) {
                *dst++ = dc;
            }
            dst += stride - 16;
        }
    }
}

void plane_16x16_pred(uint8_t *dst, int stride, int a_av, int b_av,
    const uint8_t top_samples[17], const uint8_t left_samples[17], int bitDepth) {

    int H=0, V=0;
    for (int x = 0; x < 8; x++) {
        H += (x+1) * (top_samples[9+x] - top_samples[7-x]);
    }
    for (int y = 0; y < 8; y++) {
        V += (y+1) * (left_samples[9+y] - left_samples[7-y]);
    }

    int a = 16 * (left_samples[16] + top_samples[16]);
    int b = (5 * H + 32) >> 6;
    int c = (5 * V + 32) >> 6;

    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            *dst++ = _clip3(0, (1 << bitDepth) - 1, (a+b*(x-7) + c*(y-7) + 16) >> 5);
        }
        dst += stride - 16;
    }
}






/*==========================================*/
/*========   CHROMA PREDICTION   ===========*/
/*==========================================*/

void vert_8x8_chroma_pred(uint8_t *dst_cb, uint8_t *dst_cr, int stride, int a_av, int b_av,
    const uint8_t top_samples_cb[9], const uint8_t left_samples_cb[9],
    const uint8_t top_samples_cr[9], const uint8_t left_samples_cr[9],
    int bitDepth, int chroma_at) {

    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            *dst_cb++ = top_samples_cb[1+x];
            *dst_cr++ = top_samples_cr[1+x];
        }
        dst_cb += stride - 8;
        dst_cr += stride - 8;
    }

}

void hor_8x8_chroma_pred(uint8_t *dst_cb, uint8_t *dst_cr, int stride, int a_av, int b_av,
    const uint8_t top_samples_cb[9], const uint8_t left_samples_cb[9],
    const uint8_t top_samples_cr[9], const uint8_t left_samples_cr[9],
    int bitDepth, int chroma_at) {

    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            *dst_cb++ = left_samples_cb[1+y];
            *dst_cr++ = left_samples_cr[1+y];
        }
        dst_cb += stride - 8;
        dst_cr += stride - 8;
    }

}

void dc_8x8_chroma_pred(uint8_t *dst_cb, uint8_t *dst_cr, int stride, int a_av, int b_av,
    const uint8_t top_samples_cb[9], const uint8_t left_samples_cb[9],
    const uint8_t top_samples_cr[9], const uint8_t left_samples_cr[9],
    int bitDepth, int chroma_at) {

    /* oh shit this one is annoying */

    if (!a_av && !b_av) {
        for (int y = 0; y < 8; y++) {
            for (int x = 0; x < 8; x++) {
                *dst_cb++ = 1 << (bitDepth-1);
                *dst_cr++ = 1 << (bitDepth-1);
            }
            dst_cb += stride - 8;
            dst_cr += stride - 8;
        }
    } else {
        int sum_a_0_cb, sum_a_1_cb, sum_b_0_cb, sum_b_1_cb;
        int sum_a_0_cr, sum_a_1_cr, sum_b_0_cr, sum_b_1_cr;
        if (a_av) {
            for (int y = 0; y < 4; y++) {sum_a_0_cb += left_samples_cb[1+y]; sum_a_0_cr += left_samples_cr[1+y];}
            for (int y = 0; y < 4; y++) {sum_a_1_cb += left_samples_cb[5+y]; sum_a_1_cr += left_samples_cr[5+y];}
        }
        if (b_av) {
            for (int x = 0; x < 4; x++) {sum_b_0_cb += top_samples_cb[1+x]; sum_b_0_cr += top_samples_cr[1+x];}
            for (int x = 0; x < 4; x++) {sum_b_1_cb += top_samples_cb[5+x]; sum_b_1_cr += top_samples_cr[5+x];}
        }

        int top_left_cb, top_right_cb, bottom_left_cb, bottom_right_cb;
        int top_left_cr, top_right_cr, bottom_left_cr, bottom_right_cr;
        if (a_av && b_av) {
            top_left_cb = (sum_a_0_cb + sum_b_0_cb + 4) >> 3;
            top_left_cr = (sum_a_0_cr + sum_b_0_cr + 4) >> 3;
            bottom_right_cb = (sum_a_1_cb + sum_b_1_cb + 4) >> 3;
            bottom_right_cr = (sum_a_1_cr + sum_b_1_cr + 4) >> 3;
        } else {
            if (a_av) {
                top_left_cb = (sum_a_0_cb + 2) >> 2;
                top_left_cr = (sum_a_0_cr + 2) >> 2;
                bottom_right_cb = (sum_a_1_cb + 2) >> 2;
                bottom_right_cr = (sum_a_1_cr + 2) >> 2;
            } else {
                top_left_cb = (sum_b_0_cb + 2) >> 2;
                top_left_cr = (sum_b_0_cr + 2) >> 2;
                bottom_right_cb = (sum_b_1_cb + 2) >> 2;
                bottom_right_cr = (sum_b_1_cr + 2) >> 2;
            }
        }
        if (b_av) {
            top_right_cb = (sum_b_1_cb + 2) >> 2;
            top_right_cr = (sum_b_1_cr + 2) >> 2;
        } else {
            top_right_cb = (sum_a_0_cb + 2) >> 2;
            top_right_cr = (sum_a_0_cr + 2) >> 2;
        }
        if (a_av) {
            bottom_left_cb = (sum_a_1_cb + 2) >> 2;
            bottom_left_cr = (sum_a_1_cr + 2) >> 2;
        } else {
            bottom_left_cb = (sum_b_0_cb + 2) >> 2;
            bottom_left_cr = (sum_b_0_cr + 2) >> 2;
        }


        /* bottom left */
        for (int y = 0; y < 4; y++) {
            for (int x = 0; x < 4; x++) {
                *(dst_cb + (y+4) * stride + x) = bottom_left_cb;
                *(dst_cr + (y+4) * stride + x) = bottom_left_cr;
            }
        }
        /* top left and bottom right */
        for (int y = 0; y < 4; y++) {
            for (int x = 0; x < 4; x++) {
                *(dst_cb + y*stride + x) = top_left_cb;
                *(dst_cr + y*stride + x) = top_left_cr;
                *(dst_cb + (y+4)*stride + x + 4) = bottom_right_cb;
                *(dst_cr + (y+4)*stride + x + 4) = bottom_right_cr;
            }
        }
        /* top right */
        for (int y = 0; y < 4; y++) {
            for (int x = 0; x < 4; x++) {
                *(dst_cb + y*stride + x + 4) = top_right_cb;
                *(dst_cr + y*stride + x + 4) = top_right_cr;
            }
        }
    }

}

void plane_8x8_chroma_pred(uint8_t *dst_cb, uint8_t *dst_cr, int stride, int a_av, int b_av,
    const uint8_t top_samples_cb[9], const uint8_t left_samples_cb[9],
    const uint8_t top_samples_cr[9], const uint8_t left_samples_cr[9],
    int bitDepth, int chroma_at) {

    int xCF = chroma_at == 3 ? 4 : 0;
    int yCF = chroma_at != 1 ? 4 : 0;

    int H_cb = 0, V_cb = 0, H_cr = 0, V_cr = 0;
    for (int x = 0; x <= 3+xCF; x++) {
        H_cb += (x+1) * (top_samples_cb[5+xCF+x] - top_samples_cb[3+xCF-x]);
        H_cr += (x+1) * (top_samples_cr[5+xCF+x] - top_samples_cr[3+xCF-x]);
    }
    for (int y = 0; y <= 3+yCF; y++) {
        V_cb += (y+1) * (left_samples_cb[5+yCF+y] - left_samples_cb[3+yCF-y]);
        V_cr += (y+1) * (left_samples_cr[5+yCF+y] - left_samples_cr[3+yCF-y]);
    }

    int a_cb = 16 * (left_samples_cb[8] + top_samples_cb[8]);
    int b_cb = ((34 - 29*(chroma_at==3)) * H_cb + 32) >> 6;
    int c_cb = ((34 - 29*(chroma_at!=1)) * V_cb + 32) >> 6;

    int a_cr = 16 * (left_samples_cr[8] + top_samples_cr[8]);
    int b_cr = ((34 - 29*(chroma_at==3)) * H_cr + 32) >> 6;
    int c_cr = ((34 - 29*(chroma_at!=1)) * V_cr + 32) >> 6;

    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            *dst_cb++ = _clip3(0, (1 << bitDepth) - 1, (a_cb + b_cb * (x-3-xCF) + c_cb * (y-3-yCF) + 16) >> 5);
            *dst_cr++ = _clip3(0, (1 << bitDepth) - 1, (a_cr + b_cr * (x-3-xCF) + c_cr * (y-3-yCF) + 16) >> 5);
        }
        dst_cb = dst_cb + stride - 8;
        dst_cr = dst_cr + stride - 8;
    }

}





void intra_pred_4x4(Macroblock *mb, int blkIdx, int pred_mode, CodecContext *ctx) {
    uint8_t *luma = mb->p_pic->luma;
    int stride = mb->p_pic->strideY;
    int mb_y = mb->mb_y;
    int mb_x = mb->mb_x;

    uint8_t top_samples[9];
    uint8_t left_samples[5];

    Neighbors n = derive_neighbors_4x4_luma(mb, blkIdx, ctx);
    int blkX = (blkIdx&3)<<2;
    int blkY = (blkIdx>>2)<<2;

    if (n.a_av) {
        for (int y = 0; y < 4; y++)
            left_samples[y+1] = luma[(mb_y*16 + blkY + y)*stride + mb_x*16 + blkX - 1];
    }
    if (n.b_av) {
        for (int x = 0; x < 4; x++)
            top_samples[x+1]  = luma[(mb_y*16 + blkY - 1)*stride + mb_x*16 + blkX + x];
    }
    if (n.c_av) {
        for (int x = 0; x < 4; x++)
            top_samples[x+5]  = luma[(mb_y*16 + blkY - 1)*stride + mb_x*16 + blkX + x + 4];
    } else {
        memset(&top_samples[5], top_samples[4], 4);
    }
    if (n.d_av) {
        left_samples[0]       = luma[(mb_y*16 + blkY - 1)*stride + mb_x*16 + blkX - 1];
        top_samples[0]        = left_samples[0];
    }

    intra4x4_table[pred_mode](
        &luma[(mb_y*16 + blkY)*stride + mb_x*16 + blkX], stride,
        n.a_av, n.b_av,
        top_samples, left_samples
            );
}

void intra_pred_16x16(Macroblock *mb, CodecContext *ctx) {
    uint8_t *luma = mb->p_pic->luma;
    int stride = mb->p_pic->strideY;
    int mb_y = mb->mb_y;
    int mb_x = mb->mb_x;

    uint8_t top_samples[17];
    uint8_t left_samples[17];

    Neighbors n = derive_neighbors_4x4_luma(mb, 0, ctx);

    if (n.a_av) {
        for (int i = 0; i < 16; i++)
            left_samples[i+1] = luma[(mb_y*16 + i)*stride + mb_x*16 - 1];
    }
    if (n.b_av) {
        for (int i = 0; i < 16; i++)
            top_samples[i+1]  = luma[(mb_y*16 - 1)*stride + mb_x*16 + i];
    }
    if (n.d_av) {
        left_samples[0]       = luma[(mb_y*16 - 1)*stride + mb_x*16 - 1];
        top_samples[0]        = left_samples[0];
    }

    intra16x16_table[mb->pred_mode](
        &luma[mb_y*16*stride + mb_x*16], stride,
        n.a_av, n.b_av,
        top_samples, left_samples, ctx->ps->sps->bit_depth_luma);
}


void intra_chroma_pred(Macroblock *mb, CodecContext *ctx) {
    uint8_t *cb = mb->p_pic->cb;
    uint8_t *cr = mb->p_pic->cr;
    int stride = mb->p_pic->strideC;
    int mb_x = mb->mb_x;
    int mb_y = mb->mb_y;


    uint8_t top_samples_cb[9];
    uint8_t left_samples_cb[9];
    uint8_t top_samples_cr[9];
    uint8_t left_samples_cr[9];

    Neighbors n = derive_neighbors_4x4_chroma(mb, 0, ctx);

    if (n.a_av) {
        for (int y = 0; y < 8; y++) {
            left_samples_cb[y+1] = cb[(mb_y*8 + y)*stride + mb_x*8 - 1];
            left_samples_cr[y+1] = cr[(mb_y*8 + y)*stride + mb_x*8 - 1];
        }
    }
    if (n.b_av) {
        for (int x = 0; x < 8; x++) {
            top_samples_cb[x+1]  = cb[(mb_y*8 - 1)*stride + mb_x*8 + x];
            top_samples_cr[x+1]  = cr[(mb_y*8 - 1)*stride + mb_x*8 + x];
        }
    }
    if (n.d_av) {
        left_samples_cb[0]       = cb[(mb_y*8-1)*stride + (mb_x*8 - 1)];
        left_samples_cr[0]       = cr[(mb_y*8-1)*stride + (mb_x*8 - 1)];
        top_samples_cb[0]        = left_samples_cb[0];
        top_samples_cr[0]        = left_samples_cr[0];
    }

    intra8x8_chroma_table[mb->intra_chroma_pred_mode](
        &cb[mb_y*8*stride + mb_x*8], &cr[mb_y*8*stride + mb_x*8], stride,
        n.a_av, n.b_av,
        top_samples_cb, left_samples_cb,
        top_samples_cr, left_samples_cr,
        ctx->ps->sps->bit_depth_chroma, ctx->ps->sps->chroma_format_idc);
}