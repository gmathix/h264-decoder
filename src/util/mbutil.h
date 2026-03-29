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
#define MB_TYPE_INTRA8x8    (1 <<  1)
#define MB_TYPE_INTRA16x16  (1 <<  2)
#define MB_TYPE_INTRA_PCM   (1 <<  3)
#define MB_TYPE_16x16       (1 <<  4)
#define MB_TYPE_16x8        (1 <<  5)
#define MB_TYPE_8x16        (1 <<  6)
#define MB_TYPE_8x8         (1 <<  7)
#define MB_TYPE_INTERLACED  (1 <<  8)
#define MB_TYPE_DIRECT2     (1 <<  9)
#define MB_TYPE_REF0        (1 << 10)
#define MB_TYPE_CBP         (1 << 11)
#define MB_TYPE_QUANT       (1 << 12)
#define MB_TYPE_FORWARD_MV  (1 << 13)
#define MB_TYPE_BACKWARD_MV (1 << 14)
#define MB_TYPE_BIDIR       (MB_TYPE_FORWARD_MV | MB_TYPE_BACKWARD_MV)
#define MB_TYPE_P0L0        (1 << 15)
#define MB_TYPE_P1L0        (1 << 16)
#define MB_TYPE_P0L1        (1 << 17)
#define MB_TYPE_P1L1        (1 << 18)
#define MB_TYPE_L0          (MB_TYPE_P0L0 | MB_TYPE_P1L0)
#define MB_TYPE_L1          (MB_TYPE_P0L1 | MB_TYPE_P1L1)
#define MB_TYPE_L0L1        (MB_TYPE_L0 | MB_TYPE_L1)
#define MB_TYPE_GMC         (1 << 19)
#define MB_TYPE_SKIP        (1 << 20)
#define MB_TYPE_ACPRED      (1 << 21)

/* sub macroblock types */
#define SUB_MB_TYPE_8x8     (1 << 22)
#define SUB_MB_TYPE_8x4     (1 << 23)
#define SUB_MB_TYPE_4x8     (1 << 24)
#define SUB_MB_TYPE_4x4     (1 << 25)
#define SUB_MB_TYPE_DIRECT  (1 << 26)


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
#define IS_16x16(a)        ((a) & MB_TYPE_16x16)
#define IS_16x8(a)         ((a) & MB_TYPE_16x8)
#define IS_8x16(a)         ((a) & MB_TYPE_8x16)
#define IS_8x8(a)          ((a) & MB_TYPE_8x8)
#define IS_ACPRED(a)       ((a) & MB_TYPE_ACPRED)
#define IS_QUANT(a)        ((a) & MB_TYPE_QUANT)

#define HAS_CBP(a)         ((a) & MB_TYPE_CBP)
#define HAS_FORWARD_MV(a)  ((a) & MB_TYPE_FORWARD_MV)
#define HAS_BACKWARD_MV(a) ((a) & MB_TYPE_BACKWARD_MV)
// dir == 0 means forward, dir == 1 is backward
#define HAS_MV(a, dir)     ((a) & (MB_TYPE_FORWARD_MV << (dir)))


static char* mb_type_to_string(uint32_t type) {
    if (IS_INTRA4x4   (type))  return "Intra_4x4";
    if (IS_INTRA16x16 (type))  return "Intra_16x16";
    if (IS_PCM        (type))  return "PCM";
    if (IS_INTRA_PCM  (type))  return "Intra_PCM";
    if (IS_SKIP       (type))  return "Skip";
    if (IS_INTERLACED (type))  return "Interlaced";
    if (IS_GMC        (type))  return "GMC";
    if (IS_16x16      (type))  return "16x16";
    if (IS_16x8       (type))  return "16x8";
    if (IS_8x8        (type))  return "8x8";
    if (IS_ACPRED     (type))  return "ACPRED";
    if (IS_QUANT      (type))  return "Quant";
    return "UNDEF";
}

#endif //TOY_H264_MBUTIL_H