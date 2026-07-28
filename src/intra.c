//
// Created by gmathix on 4/6/26.
//

#include "intra.h"


#include "picture.h"
#include "util/mbutil.h"


static const intra_pred_func        intra4x4_table[9];
static const intra_pred_func        intra8x8_table[9];
static const intra_pred_func        intra16x16_table[4];
static const intra_pred_chroma_func intra8x8_chroma_table[4];




static ALWAYS_INLINE int sum(const uint8_t *samples, int start, int end) {
    int sum = 0;
    for (int i = start; i <= end; i++) {
        sum += samples[i];
    }
    return sum;
}


/*=======================================*/
/*========   4x4 PREDICTION   ===========*/
/*=======================================*/

void vert_4x4_pred(uint8_t *dst, int stride,  int a_av, int b_av,
    const uint8_t *top, const uint8_t *left) {

    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            dst[x] = top[1+x];
        }
        dst += stride;
    }
}

void hor_4x4_pred(uint8_t *dst, int stride,  int a_av, int b_av,
    const uint8_t *top, const uint8_t *left) {

    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            dst[x] = left[1+y];
        }
        dst += stride;
    }
}

void dc_4x4_pred(uint8_t *dst, int stride,  int a_av, int b_av,
    const uint8_t *top, const uint8_t *left) {

    int dc;
    if (a_av && b_av) {
        dc = (top[1] + top[2] + top[3] + top[4] +
              left[1] + left[2] + left[3] + left[4] + 4) >> 3;
    } else if (a_av && !b_av) {
        dc = (left[1] + left[2] + left[3] + left[4] + 2) >> 2;
    } else if (!a_av && b_av) {
        dc = (top[1] + top[2] + top[3] + top[4] + 2) >> 2;
    } else {
        dc = 1 << (8-1); // just assume BitDepthY = 8 because i don't want to add CodecContext everywhere here
    }
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            dst[x] = (int16_t)dc;
        }
        dst += stride;
    }
}

void diag_down_left_4x4_pred(uint8_t *dst, int stride,  int a_av, int b_av,
    const uint8_t *top, const uint8_t *left) {

    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            if (y == 3 && x == 3)
                dst[x] = (top[7] + 3*top[8] + 2) >> 2;
            else
                dst[x] = (top[1+x+y] + 2*top[1+x+y+1] + top[1+x+y+2] + 2) >> 2;
        }
        dst += stride;
    }
}

void diag_down_right_4x4_pred(uint8_t *dst, int stride,  int a_av, int b_av,
    const uint8_t *top, const uint8_t *left) {

    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            if      (x == y)   dst[x] = (top[1] + 2*top[0] + left[1] + 2) >> 2;
            else if (x > y)    dst[x] = (top[1+x-y-2] + 2*top[1+x-y-1] + top[1+x-y] + 2) >> 2;
            else               dst[x] = (left[1+y-x-2] + 2*left[1+y-x-1] + left[1+y-x] + 2) >> 2;
        }
        dst += stride;
    }
}

void vert_right_4x4_pred(uint8_t *dst, int stride,  int a_av, int b_av,
    const uint8_t *top, const uint8_t *left) {

    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            int zVR = 2*x-y;
            if (zVR >= 0) {
                if (!(zVR&1)) {
                    dst[x] =
                        (top[x-(y>>1)]+top[1+x-(y>>1)] + 1) >> 1;
                } else if (zVR&1) {
                    dst[x] =
                        (top[x-(y>>1)-1] + 2*top[x-(y>>1)] + top[1+x-(y>>1)] + 2) >> 2;
                }
            }
            else if (zVR == -1) {
                dst[x] =
                    (left[1] + 2*left[0] + top[1] + 2) >> 2;
            } else {
                dst[x] =
                    (left[y] + 2*left[y-1] + left[y-2] + 2) >> 2;
            }
        }
        dst += stride;
    }
}

void hor_down_4x4_pred(uint8_t *dst, int stride,  int a_av, int b_av,
    const uint8_t *top, const uint8_t *left) {

    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            int zHD = 2*y-x;
            if (zHD >= 0) {
                if (!(zHD&1)) {
                    dst[x] =
                        (left[y-(x>>1)] + left[1+y-(x>>1)] + 1) >> 1;
                } else if (zHD&1) {
                    dst[x] =
                        (left[y-(x>>1)-1] + 2*left[y-(x>>1)] + left[1+y-(x>>1)] + 2) >> 2;
                }
            }
            else if (zHD == -1) {
                dst[x] =
                    (left[1] + 2*left[0] + top[1] + 2) >> 2;
            } else {
                dst[x] =
                    (top[x] + 2*top[x-1] + top[x-2] + 2) >> 2;
            }
        }
        dst += stride;
    }
}

void vert_left_4x4_pred(uint8_t *dst, int stride,  int a_av, int b_av,
    const uint8_t *top, const uint8_t *left) {

    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            if (y&1) {
                dst[x] = (top[1+x+(y>>1)] + 2*top[2+x+(y>>1)] + top[3+x+(y>>1)] + 2) >> 2;
            } else {
                dst[x] = (top[1+x+(y>>1)] + top[2+x+(y>>1)] + 1) >> 1;
            }
        }
        dst += stride;
    }
}

void hor_up_4x4_pred(uint8_t *dst, int stride,  int a_av, int b_av,
    const uint8_t *top, const uint8_t *left) {

    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            int zHU = x+2*y;
            if (zHU < 5) {
                if (!(zHU&1)) {
                    dst[x] =
                        (left[1+y+(x>>1)] + left[2+y+(x>>1)] + 1) >> 1;
                } else if (zHU&1) {
                    dst[x] =
                        (left[1+y+(x>>1)] + 2*left[2+y+(x>>1)] + left[3+y+(x>>1)] + 2) >> 2;
                }
            } else if (zHU == 5) {
                dst[x] =
                    (left[3] + 3*left[4] + 2) >> 2;
            } else {
                dst[x] = left[4];
            }
        }
        dst += stride;
    }
}



/*=======================================*/
/*========   8x8 PREDICTION   ===========*/
/*=======================================*/

void filter_samples(const uint8_t top_temp[16], const uint8_t left_temp[9],
        uint8_t *top, uint8_t *left, int a_av, int b_av, int c_av, int d_av) {
    
    if (b_av) {
        top[1] = d_av
            ? (top_temp[0] + 2*top_temp[1] + top_temp[2] + 2) >> 2
            : (3*top_temp[1] + top_temp[2] + 2) >> 2;
        top[16] = (top_temp[15] + 3*top_temp[16] + 2) >> 2;
        for (int x = 1; x <= 14; x++) {
            top[1+x] = (top_temp[x] + 2*top_temp[1+x] + top_temp[x+2] + 2) >> 2;
        }
    }
    if (d_av) {
        if (!a_av || !b_av) {
            if (b_av) {
                top[0] = (3*top_temp[0] + top_temp[1] + 2) >> 2;
            } else if (a_av) {
                top[0] = (3*top_temp[0] + left_temp[1] + 2) >> 2;
            } else {
                top[0] = top_temp[0];
            }
        } else {
            top[0] = (top_temp[1] + 2*top_temp[0] + left_temp[1] + 2) >> 2;
        }
        left[0] = top[0];
    }
    if (a_av) {
        left[1] = d_av
            ? (left_temp[0] + 2*left_temp[1] + left_temp[2] + 2) >> 2
            : (3*left_temp[1] + left_temp[2] + 2) >> 2;
        left[8] = (left_temp[7] + 3*left_temp[8] + 2) >> 2;
        for (int y = 1; y <= 6; y++) {
            left[1+y] = (left_temp[y] + 2*left_temp[1+y] + left_temp[y+2] + 2) >> 2;
        }
    }
}

void vert_8x8_pred(uint8_t *dst, int stride, int a_av, int b_av,
        const uint8_t *top, const uint8_t *left) {

    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            dst[x] = top[1+x];
        }
        dst += stride;
    }
}

void hor_8x8_pred(uint8_t *dst, int stride,  int a_av, int b_av,
        const uint8_t *top, const uint8_t *left) {

    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            dst[x] = left[1+y];
        }
        dst += stride;
    }
}

void dc_8x8_pred(uint8_t *dst, int stride,  int a_av, int b_av,
        const uint8_t *top, const uint8_t *left) {


    int dc;
    if (a_av && b_av) {
        dc = (sum(top, 1, 8) + sum(left, 1, 8) + 8) >> 4;
    } else if (!b_av && a_av) {
        dc = (sum(left, 1, 8) + 4) >> 3;
    } else if (!a_av && b_av) {
        dc = (sum(top, 1, 8) + 4) >> 3;
    } else {
        dc = 1 << (8-1);
    }

    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            dst[x] = (uint8_t) dc;
        }
        dst += stride;
    }
}

void diag_down_left_8x8_pred(uint8_t *dst, int stride, int a_av, int b_av,
        const uint8_t *top, const uint8_t *left) {

    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            if (x == 7 && y == 7) {
                dst[x] = (top[15] + 3*top[16] + 2) >> 2;
            } else {
                dst[x] = (top[1+x+y] + 2*top[x+y+2] + top[x+y+3] + 2) >> 2;
            }
        }
        dst += stride;
    }
}

void diag_down_right_8x8_pred(uint8_t *dst, int stride, int a_av, int b_av,
        const uint8_t *top, const uint8_t *left) {

    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            if (x > y) {
                dst[x] = (top[x-y-1] + 2*top[x-y] + top[x-y+1] + 2) >> 2;
            } else if (x < y) {
                dst[x] = (left[y-x-1] + 2*left[y-x] + left[y-x+1] + 2) >> 2;
            } else {
                dst[x] = (top[1] + 2*top[0] + left[1] + 2) >> 2;
            }
        }
        dst += stride;
    }
}

void vert_right_8x8_pred(uint8_t *dst, int stride,  int a_av, int b_av,
        const uint8_t *top, const uint8_t *left) {

    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            int zVR = 2*x-y;
            if (zVR >= 0 && zVR % 2 == 0) {
                dst[x] = (top[x-(y>>1)] + top[x-(y>>1)+1] + 1) >> 1;
            } else if (zVR > 0 && zVR % 2 == 1) {
                dst[x] = (top[x-(y>>1)-1] + 2*top[x-(y>>1)] + top[x-(y>>1)+1] + 2) >> 2;
            } else if (zVR == -1) {
                dst[x] = (left[1] + 2*left[0] + top[1] + 2) >> 2;
            } else {
                dst[x] = (left[y-2*x] + 2*left[y-2*x-1] + left[y-2*x-2] + 2) >> 2;
            }
        }
        dst += stride;
    }
}

void hor_down_8x8_pred(uint8_t *dst, int stride,  int a_av, int b_av,
        const uint8_t *top, const uint8_t *left) {

    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            int zHD = 2*y-x;
            if (zHD >= 0 && zHD % 2 == 0) {
                dst[x] = (left[y-(x>>1)] + left[y-(x>>1)+1] + 1) >> 1;
            } else if (zHD > 0 && zHD % 2 == 1) {
                int t = (left[y-(x>>1)-1] + 2*left[y-(x>>1)] + left[y-(x>>1)+1] + 2) >> 2;
                dst[x] = t;
            } else if (zHD == -1) {
                dst[x] = (left[1] + 2*left[0] + top[1] + 2) >> 2;
            } else {
                dst[x] = (top[x-2*y] + 2*top[x-2*y-1] + top[x-2*y-2] + 2) >> 2;
            }
        }
        dst += stride;
    }
}

void vert_left_8x8_pred(uint8_t *dst, int stride,  int a_av, int b_av,
        const uint8_t *top, const uint8_t *left) {

    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            if (y % 2 == 0) {
                dst[x] = (top[x+(y>>1)+1] + top[x+(y>>1)+2] + 1) >> 1;
            } else {
                dst[x] = (top[x+(y>>1)+1] + 2*top[x+(y>>1)+2] + top[x+(y>>1)+3] + 2) >> 2;
            }
        }
        dst += stride;
    }
}

void hor_up_8x8_pred(uint8_t *dst, int stride,  int a_av, int b_av,
        const uint8_t *top, const uint8_t *left) {

    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            int zHU = x+2*y;
            if (zHU % 2 == 0 && zHU <= 12) {
                dst[x] = (left[y+(x>>1)+1] + left[y+(x>>1)+2] + 1) >> 1;
            } else if (zHU < 13) {
                dst[x] = (left[y+(x>>1)+1] + 2*left[y+(x>>1)+2] + left[y+(x>>1)+3] + 2) >> 2;
            } else if (zHU == 13) {
                dst[x] = (left[7] + 3*left[8] + 2) >> 2;
            } else {
                dst[x] = left[8];
            }
        }
        dst += stride;
    }
}



/*=========================================*/
/*========   16x16 PREDICTION   ===========*/
/*=========================================*/

void vert_16x16_pred(uint8_t *dst, int stride, int a_av, int b_av,
    const uint8_t *top, const uint8_t *left) {

    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            dst[x] = top[x+1];
        }
        dst += stride;
    }
}


void hor_16x16_pred(uint8_t *dst, int stride, int a_av, int b_av,
    const uint8_t *top, const uint8_t *left) {

    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            dst[x] = left[y+1];
        }
        dst += stride;
    }
}

void dc_16x16_pred(uint8_t *dst, int stride, int a_av, int b_av,
    const uint8_t *top, const uint8_t *left) {

    if (!a_av && !b_av) {
        for (int y = 0; y < 16; y++) {
            for (int x = 0; x < 16; x++) {
                dst[x] = 1 << (8-1);
            }
            dst += stride;
        }
    } else {
        int sum = 0;
        int32_t dc;

        if (a_av && b_av) {
            for (int i = 0; i < 16; i++) sum += top[i+1];
            for (int i = 0; i < 16; i++) sum += left[i+1];
            dc = (sum + 16) >> 5;
        } else if (a_av) {
            for (int i = 0; i < 16; i++) sum += left[i+1];
            dc = (sum + 8) >> 4;
        } else {
            for (int i = 0; i < 16; i++) sum += top[i+1];
            dc = (sum + 8) >> 4;
        }

        for (int y = 0; y < 16; y++) {
            for (int x = 0; x < 16; x++) {
                dst[x] = dc;
            }
            dst += stride;
        }
    }
}

void plane_16x16_pred(uint8_t *dst, int stride, int a_av, int b_av,
    const uint8_t *top, const uint8_t *left) {

    int H=0, V=0;
    for (int x = 0; x < 8; x++) {
        H += (x+1) * (top[9+x] - top[7-x]);
    }
    for (int y = 0; y < 8; y++) {
        V += (y+1) * (left[9+y] - left[7-y]);
    }

    int a = 16 * (left[16] + top[16]);
    int b = (5 * H + 32) >> 6;
    int c = (5 * V + 32) >> 6;

    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            dst[x] = _clip3(0, (1 << 8) - 1, (a+b*(x-7) + c*(y-7) + 16) >> 5);
        }
        dst += stride;
    }
}






/*==========================================*/
/*========   CHROMA PREDICTION   ===========*/
/*==========================================*/

void vert_8x8_chroma_pred(uint8_t *dst_cb, uint8_t *dst_cr, int stride, int a_av, int b_av,
    const uint8_t *top_cb, const uint8_t *left_cb,
    const uint8_t *top_cr, const uint8_t *left_cr,
    int bitDepth, int chroma_at) {

    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            dst_cb[x] = top_cb[1+x];
            dst_cr[x] = top_cr[1+x];
        }
        dst_cb += stride;
        dst_cr += stride;
    }

}

void hor_8x8_chroma_pred(uint8_t *dst_cb, uint8_t *dst_cr, int stride, int a_av, int b_av,
    const uint8_t *top_cb, const uint8_t *left_cb,
    const uint8_t *top_cr, const uint8_t *left_cr,
    int bitDepth, int chroma_at) {

    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            dst_cb[x] = left_cb[1+y];
            dst_cr[x] = left_cr[1+y];
        }
        dst_cb += stride;
        dst_cr += stride;
    }

}

void dc_8x8_chroma_pred(uint8_t *dst_cb, uint8_t *dst_cr, int stride, int a_av, int b_av,
    const uint8_t *top_cb, const uint8_t *left_cb,
    const uint8_t *top_cr, const uint8_t *left_cr,
    int bitDepth, int chroma_at) {

    /* oh shit this one is annoying */

    if (!a_av && !b_av) {
        for (int y = 0; y < 8; y++) {
            for (int x = 0; x < 8; x++) {
                dst_cb[x] = 1 << (bitDepth-1);
                dst_cr[x] = 1 << (bitDepth-1);
            }
            dst_cb += stride;
            dst_cr += stride;
        }
    } else {
        int sum_a_0_cb = 0, sum_a_1_cb = 0, sum_b_0_cb = 0, sum_b_1_cb = 0;
        int sum_a_0_cr = 0, sum_a_1_cr = 0, sum_b_0_cr = 0, sum_b_1_cr = 0;
        if (a_av) {
            for (int y = 0; y < 4; y++) {sum_a_0_cb += left_cb[1+y]; sum_a_0_cr += left_cr[1+y];}
            for (int y = 0; y < 4; y++) {sum_a_1_cb += left_cb[5+y]; sum_a_1_cr += left_cr[5+y];}
        }
        if (b_av) {
            for (int x = 0; x < 4; x++) {sum_b_0_cb += top_cb[1+x]; sum_b_0_cr += top_cr[1+x];}
            for (int x = 0; x < 4; x++) {sum_b_1_cb += top_cb[5+x]; sum_b_1_cr += top_cr[5+x];}
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
    const uint8_t *top_cb, const uint8_t *left_cb,
    const uint8_t *top_cr, const uint8_t *left_cr,
    int bitDepth, int chroma_at) {

    int xCF = chroma_at == 3 ? 4 : 0;
    int yCF = chroma_at != 1 ? 4 : 0;

    int H_cb = 0, V_cb = 0, H_cr = 0, V_cr = 0;
    for (int x = 0; x <= 3+xCF; x++) {
        H_cb += (x+1) * (top_cb[5+xCF+x] - top_cb[3+xCF-x]);
        H_cr += (x+1) * (top_cr[5+xCF+x] - top_cr[3+xCF-x]);
    }
    for (int y = 0; y <= 3+yCF; y++) {
        V_cb += (y+1) * (left_cb[5+yCF+y] - left_cb[3+yCF-y]);
        V_cr += (y+1) * (left_cr[5+yCF+y] - left_cr[3+yCF-y]);
    }

    int a_cb = 16 * (left_cb[8] + top_cb[8]);
    int b_cb = ((34 - 29*(chroma_at==3)) * H_cb + 32) >> 6;
    int c_cb = ((34 - 29*(chroma_at!=1)) * V_cb + 32) >> 6;

    int a_cr = 16 * (left_cr[8] + top_cr[8]);
    int b_cr = ((34 - 29*(chroma_at==3)) * H_cr + 32) >> 6;
    int c_cr = ((34 - 29*(chroma_at!=1)) * V_cr + 32) >> 6;

    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            dst_cb[x] = _clip3(0, (1 << bitDepth) - 1, (a_cb + b_cb * (x-3-xCF) + c_cb * (y-3-yCF) + 16) >> 5);
            dst_cr[x] = _clip3(0, (1 << bitDepth) - 1, (a_cr + b_cr * (x-3-xCF) + c_cr * (y-3-yCF) + 16) >> 5);
        }
        dst_cb = dst_cb + stride;
        dst_cr = dst_cr + stride;
    }

}





static const intra_pred_func intra4x4_table[9] = {
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

static const intra_pred_func intra8x8_table[9] = {
    vert_8x8_pred,
    hor_8x8_pred,
    dc_8x8_pred,
    diag_down_left_8x8_pred,
    diag_down_right_8x8_pred,
    vert_right_8x8_pred,
    hor_down_8x8_pred,
    vert_left_8x8_pred,
    hor_up_8x8_pred
};

static const intra_pred_func intra16x16_table[4] = {
    vert_16x16_pred,
    hor_16x16_pred,
    dc_16x16_pred,
    plane_16x16_pred
};

static const intra_pred_chroma_func intra8x8_chroma_table[4] = {
    dc_8x8_chroma_pred,
    hor_8x8_chroma_pred,
    vert_8x8_chroma_pred,
    plane_8x8_chroma_pred
};



void intra_pred_4x4(Macroblock *mb, int blkIdx, int pred_mode, CodecContext *ctx) {
    uint8_t *luma = mb->p_pic->luma;
    int stride = mb->p_pic->widthY;
    int mb_y = mb->mb_y;
    int mb_x = mb->mb_x;
    int blkX = (blkIdx&3)<<2;
    int blkY = (blkIdx>>2)<<2;
    int base_y = mb_y * 16 + blkY;
    int base_x = mb_x * 16 + blkX;

    uint8_t top[9];
    uint8_t left[5];

    Neighbors n = derive_neighbors_4x4(mb, blkIdx, ctx);

    bool constrainedPred = mb->p_pic->sh->pps->constrained_intra_pred_flag;
    bool a_av = n.a.av && !(IS_INTER(mb->p_pic->mb_types[mb->mbAddr + n.a.mb_off]) && constrainedPred);
    bool b_av = n.b.av && !(IS_INTER(mb->p_pic->mb_types[mb->mbAddr + n.b.mb_off]) && constrainedPred);
    bool c_av = n.c.av && !(IS_INTER(mb->p_pic->mb_types[mb->mbAddr + n.c.mb_off]) && constrainedPred);
    bool d_av = n.d.av && !(IS_INTER(mb->p_pic->mb_types[mb->mbAddr + n.d.mb_off]) && constrainedPred);


    if (a_av) {
        for (int y = 0; y < 4; y++)
            left[y+1] = luma[(base_y + y)*stride + base_x- 1];
    }
    if (b_av) {
        for (int x = 0; x < 4; x++)
            top[x+1]  = luma[(base_y - 1)*stride + base_x + x];
    }
    if (c_av) {
        for (int x = 0; x < 4; x++)
            top[x+5]  = luma[(base_y - 1)*stride + base_x + x + 4];
    } else if (b_av) {
        memset(&top[5], top[4], 4);
    }
    if (d_av) {
        left[0]       = luma[(base_y - 1)*stride + base_x - 1];
        top[0]        = left[0];
    }

    intra4x4_table[pred_mode](
        &luma[base_y*stride + base_x], stride,
        a_av, b_av,
        top, left
        );
}

void intra_pred_8x8(Macroblock *mb, int idx8x8, int pred_mode, CodecContext *ctx) {
    uint8_t *luma = mb->p_pic->luma;
    int stride = mb->p_pic->widthY;
    int mb_y = mb->mb_y;
    int mb_x = mb->mb_x;
    int blkY = (idx8x8 / 2) * 8;
    int blkX = (idx8x8 % 2) * 8;
    int base_y = mb_y * 16 + blkY;
    int base_x = mb_x * 16 + blkX;

    uint8_t left_temp[9];
    uint8_t top_temp [17];
    uint8_t left[9];
    uint8_t top[17];

    Neighbors n = derive_neighbors_2x2(mb, idx8x8, ctx);

    bool constrainedPred = mb->p_pic->sh->pps->constrained_intra_pred_flag;
    bool a_av = n.a.av && !(IS_INTER(mb->p_pic->mb_types[mb->mbAddr + n.a.mb_off]) && constrainedPred);
    bool b_av = n.b.av && !(IS_INTER(mb->p_pic->mb_types[mb->mbAddr + n.b.mb_off]) && constrainedPred);
    bool c_av = n.c.av && !(IS_INTER(mb->p_pic->mb_types[mb->mbAddr + n.c.mb_off]) && constrainedPred);
    bool d_av = n.d.av && !(IS_INTER(mb->p_pic->mb_types[mb->mbAddr + n.d.mb_off]) && constrainedPred);

    if (a_av) {
        for (int y = 0; y < 8; y++)
            left_temp[1+y] = luma[(base_y + y) * stride + base_x - 1];
    }
    if (b_av) {
        for (int x = 0; x < 8; x++)
            top_temp[1+x] = luma[(base_y - 1)*stride + base_x + x];
    }
    if (c_av) {
        for (int x = 8; x < 16; x++)
            top_temp[1+x] = luma[(base_y - 1)*stride + base_x + x];
    } else if (b_av) {
        memset(&top_temp[9], top_temp[8], 8);
    }
    if (d_av) {
        left_temp[0] = luma[(base_y - 1)*stride  + base_x - 1];
        top_temp[0]  = luma[(base_y - 1)*stride  + base_x - 1];
    }

    filter_samples(top_temp, left_temp, top, left, a_av, b_av, c_av, d_av);

    intra8x8_table[pred_mode](
        &luma[base_y*stride + base_x], stride,
        a_av, b_av,
        top, left
        );
}

void intra_pred_16x16(Macroblock *mb, CodecContext *ctx) {
    uint8_t *luma = mb->p_pic->luma;
    int stride = mb->p_pic->widthY;
    int mb_y = mb->mb_y;
    int mb_x = mb->mb_x;

    uint8_t top[17];
    uint8_t left[17];

    Neighbors n = derive_neighbors_4x4(mb, 0, ctx);

    bool constrainedPred = mb->p_pic->sh->pps->constrained_intra_pred_flag;
    bool a_av = n.a.av && !(IS_INTER(mb->p_pic->mb_types[mb->mbAddr + n.a.mb_off]) && constrainedPred);
    bool b_av = n.b.av && !(IS_INTER(mb->p_pic->mb_types[mb->mbAddr + n.b.mb_off]) && constrainedPred);
    bool c_av = n.c.av && !(IS_INTER(mb->p_pic->mb_types[mb->mbAddr + n.c.mb_off]) && constrainedPred);
    bool d_av = n.d.av && !(IS_INTER(mb->p_pic->mb_types[mb->mbAddr + n.d.mb_off]) && constrainedPred);

    if (a_av) {
        for (int i = 0; i < 16; i++)
            left[i+1] = luma[(mb_y*16 + i)*stride + mb_x*16 - 1];
    }
    if (b_av) {
        for (int i = 0; i < 16; i++)
            top[i+1]  = luma[(mb_y*16 - 1)*stride + mb_x*16 + i];
    }
    if (d_av) {
        left[0]       = luma[(mb_y*16 - 1)*stride + mb_x*16 - 1];
        top[0]        = left[0];
    }

    int pred_mode = ctx->mb_metadata[mb->mbAddr].intra_16x16_pred_mode;
    intra16x16_table[pred_mode](
        &luma[mb_y*16*stride + mb_x*16], stride,
        a_av, b_av,
        top, left);
}


void intra_chroma_pred(Macroblock *mb, CodecContext *ctx) {
    uint8_t *cb = mb->p_pic->cb;
    uint8_t *cr = mb->p_pic->cr;
    int stride = mb->p_pic->widthC;
    int mb_x = mb->mb_x;
    int mb_y = mb->mb_y;


    uint8_t top_cb[9];
    uint8_t left_cb[9];
    uint8_t top_cr[9];
    uint8_t left_cr[9];

    Neighbors n = derive_neighbors_2x2(mb, 0, ctx);

    if (n.a.av) {
        for (int y = 0; y < 8; y++) {
            left_cb[y+1] = cb[(mb_y*8 + y)*stride + mb_x*8 - 1];
            left_cr[y+1] = cr[(mb_y*8 + y)*stride + mb_x*8 - 1];
        }
    }
    if (n.b.av) {
        for (int x = 0; x < 8; x++) {
            top_cb[x+1]  = cb[(mb_y*8 - 1)*stride + mb_x*8 + x];
            top_cr[x+1]  = cr[(mb_y*8 - 1)*stride + mb_x*8 + x];
        }
    }
    if (n.d.av) {
        left_cb[0]       = cb[(mb_y*8-1)*stride + (mb_x*8 - 1)];
        left_cr[0]       = cr[(mb_y*8-1)*stride + (mb_x*8 - 1)];
        top_cb[0]        = left_cb[0];
        top_cr[0]        = left_cr[0];
    }

    int pred_mode = ctx->mb_metadata[mb->mbAddr].intra_chroma_pred_mode;
    intra8x8_chroma_table[pred_mode](
        &cb[mb_y*8*stride + mb_x*8], &cr[mb_y*8*stride + mb_x*8], stride,
        n.a.av, n.b.av,
        top_cb, left_cb,
        top_cr, left_cr,
        ctx->ps->sps->bit_depth_chroma, ctx->ps->sps->chroma_format_idc);
}