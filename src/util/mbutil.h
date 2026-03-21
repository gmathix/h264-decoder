//
// Created by gmathix on 3/21/26.
//

#ifndef TOY_H264_MBUTIL_H
#define TOY_H264_MBUTIL_H


#include <stdint.h>


/* picture type */
#define PICT_TOP_FIELD     1
#define PICT_BOTTOM_FIELD  2
#define PICT_FRAME         3



/* macroblock types */
#define MB_TYPE_INTRA4x4    (1 <<  0)
#define MB_TYPE_INTRA16x16  (1 <<  1)
#define MB_TYPE_INTRA_PCM   (1 <<  2)
#define MB_TYPE_16x16       (1 <<  3)
#define MB_TYPE_16x8        (1 <<  4)
#define MB_TYPE_8x16        (1 <<  5)
#define MB_TYPE_8x8         (1 <<  6)
#define MB_TYPE_INTERLACED  (1 <<  7)
#define MB_TYPE_DIRECT2     (1 <<  8)
#define MB_TYPE_REF0        (1 <<  9)
#define MB_TYPE_CBP         (1 << 10)
#define MB_TYPE_QUANT       (1 << 11)
#define MB_TYPE_FORWARD_MV  (1 << 12)
#define MB_TYPE_BACKWARD_MV (1 << 13)
#define MB_TYPE_BIDIR       (MB_TYPE_FORWARD_MV | MB_TYPE_BACKWARD_MV)
#define MB_TYPE_P0L0        (1 << 14)
#define MB_TYPE_P1L0        (1 << 15)
#define MB_TYPE_P0L1        (1 << 16)
#define MB_TYPE_P1L1        (1 << 17)
#define MB_TYPE_L0          (MB_TYPE_P0L0 | MB_TYPE_P1L0)
#define MB_TYPE_L1          (MB_TYPE_P0L1 | MB_TYPE_P1L1)
#define MB_TYPE_L0L1        (MB_TYPE_L0 | MB_TYPE_L1)
#define MB_TYPE_GMC         (1 << 18)
#define MB_TYPE_SKIP        (1 << 19)
#define MB_TYPE_ACPRED      (1 << 20)


#define IS_INTRA4x4(a)     ((a) & MB_TYPE_INTRA4x4)
#define IS_INTRA16x16(a)   ((a) & MB_TYPE_INTRA16x16)
#define IS_PCM(a)          ((a) & MB_TYPE_INTRA_PCM)
#define IS_INTRA(a)        ((a) & 7)
#define IS_INTER(a)        ((a) & (MB_TYPE_16x16 | MB_TYPE_16x8 | \
                                   MB_TYPE_8x16  | MB_TYPE_8x8))
#define IS_SKIP(a)         ((a) & MB_TYPE_SKIP)
#define IS_INTRA_PCM(a)    ((a) & MB_TYPE_INTRA_PCM)
#define IS_INTERLACED(a)   ((a) & MB_TYPE_INTERLACED)
#define IS_DIRECT(a)       ((a) & MB_TYPE_DIRECT2)
#define IS_GMC(a)          ((a) & MB_TYPE_GMC)
#define IS_16X16(a)        ((a) & MB_TYPE_16x16)
#define IS_16X8(a)         ((a) & MB_TYPE_16x8)
#define IS_8X16(a)         ((a) & MB_TYPE_8x16)
#define IS_8X8(a)          ((a) & MB_TYPE_8x8)
#define IS_ACPRED(a)       ((a) & MB_TYPE_ACPRED)
#define IS_QUANT(a)        ((a) & MB_TYPE_QUANT)

#define HAS_CBP(a)         ((a) & MB_TYPE_CBP)
#define HAS_FORWARD_MV(a)  ((a) & MB_TYPE_FORWARD_MV)
#define HAS_BACKWARD_MV(a) ((a) & MB_TYPE_BACKWARD_MV)
// dir == 0 means forward, dir == 1 is backward
#define HAS_MV(a, dir)     ((a) & (MB_TYPE_FORWARD_MV << (dir)))

#endif //TOY_H264_MBUTIL_H