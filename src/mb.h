//
// Created by gmathix on 3/21/26.
//

#ifndef TOY_H264_MB_H
#define TOY_H264_MB_H

#include <stdint.h>


#include "nal.h"
#include "slice.h"
#include "util/formulas.h"


typedef struct I_MbInfo {
    int type;
    int pred_mode;
    int cbp_chroma;
    int cbp_luma;
} I_MbInfo ;

extern const I_MbInfo i_mb_type_info[26];


typedef struct P_MbInfo {
    int type;
    uint8_t  part_count;
    uint8_t  mb_part_width;
    uint8_t  mb_part_height;
} P_MbInfo ;

extern const P_MbInfo p_mb_type_info[5];
extern const P_MbInfo p_sub_mb_type_info[4];


typedef struct B_MbInfo {
    int type;
    uint8_t  part_count;
    uint8_t  mb_part_width;
    uint8_t  mb_part_height;
} B_MbInfo ;

extern const B_MbInfo b_mb_type_info[23];
extern const B_MbInfo b_sub_mb_type_info[13];

extern const int sub_width_c_info[4];
extern const int sub_height_c_info[4];


extern const int luma_location_diff[4][2];



typedef struct {
    int type;
    uint32_t intra_chroma_pred_mode;
    int32_t mb_qp_delta;
    int16_t luma_total_coeffs[16]; /* FIXME: CABAC uses total_coeffs per 8x8 block
                                        needs a total_coeff array for every union
                                        fine for now as CAVAC doesnt support 8x8 transforms at all */
    union {
        /* Intra 4x4 / 8x8 */
        struct {
            int16_t luma_4x4_coeffs[16][16];
            int16_t luma_8x8_coeffs[4][64]; // just to avoid undefined behavior in residual_luma
        };
        /* Intra 16x16 */
        struct {
            int16_t luma_16x16_DC[16];
            int16_t luma_16x16_AC[16][15];
        };
    };

    int16_t chroma_total_coeffs[4];
    int16_t chroma_DC[2][4];
    int16_t chroma_AC[2][4][15];
} Macroblock ;



typedef void (*residual_function)(Macroblock*, int, int, int, int16_t[], int, int, int, SliceHeader*, CodecContext*);



void  decode_macroblock        (Macroblock *mb_array, int mbAddr, SliceHeader *sh, NalUnit *nal_unit, CodecContext *ctx);
void  mb_pred                  (Macroblock *mb_array, int mbAddr, int type, int pred_mode, SliceHeader *sh, NalUnit *nal_unit, CodecContext *ctx);

void  residual                 (Macroblock *mb_array, int mbAddr, int type, int t_8x8_flag, int startIdx, int endIdx, int cbp_luma, int cbp_chroma,
                                    residual_function residual_block, SliceHeader *sh, CodecContext *ctx);

void  residual_luma            (Macroblock *mb_array, int mbAddr, int type, int t_8x8_flag, int cbp_luma,
                                int startIdx, int endIdx,
                                residual_function residual_block, SliceHeader *sh, CodecContext *ctx);

void  residual_block_cabac     (Macroblock *mb_array, int mbAddr, int blkIdx, int pbt, int16_t coeffLevel[], int startIdx, int endIdx, int maxNumCoeff, SliceHeader *sh, CodecContext *ctx);



void derive_neighbor_4x4_luma  (int luma4x4BlkIdx, int currMbAddr, int *mbAddrA, int *luma4x4BlkIdxA, int *mbAddrB, int *luma4x4BlkIdxB, SliceHeader *sh);
void derive_neighbor           (bool luma, Coord location, int currMbAddr, int *mbAddrN, Coord *xWyW, SliceHeader *sh);

/* little luma/chroma coeff tutorial */

/*

*** LUMA BLOCKS INSIDE A MACROBLOCK ***

    a macroblock is 16x16 luma -> 16 x (4x4 blocks)

    layout:

    [ 0] [ 1] [ 2] [ 3]
    [ 4] [ 5] [ 6] [ 7]    -> each of those blocks has its own transform coeffs
    [ 8] [ 9] [10] [11]       so residual parsing fills int16_t coeff[16][16]
    [12] [13] [14] [15]


    transform coeffs are not coded row-by-row but scanned in zigzag order, for example

     0  1  4  5
     2  3  6  7    -> CAVLC decodes coeff[zigzag[i]]
     8  9 12 13
    10 11 14 15



*** DC vs AC COEFFICIENTS ***

    in every transform block : [0][0] = DC
    this is the average intensity of the residual block
    everything else : AC coefficients represent higher spatial frequencies

    DC is treated specially because neighbor prediction already removes DC energy
    but for Intra16x16, DC is handled differently :

        instead of 16 separate DC values :
           1. each 4x4 block still has AC coefficients
           2. the 16 DC values are grouped
           3. a secondary 4x4 transform is applied to those DCs

        so you parse 16 AC blocks + 1 DC block (16 coeffs)
        that DC block goes through a Hadamard transform, not the normal one


*** CHROMA RESIDUALS ***

chroma blocks depend on subsampling
for the most common case 4:2:0, 1 luma macroblock correponds to 2 chroma planes (Cb, Cr)
   each of size 8x8, split into 4 x (4x4 blocks)

so per macroblock : 4 Cb plane blocks, 4 Cr plane blocks
each has AC blocks, plus a 2x2 DC block


*/

static inline int derive_4x4_luma_blk_scan_order(int xP, int yP) {
    return 8 * (yP / 8) + 4 * (xP / 8) +
            2 * ((yP % 8) / 4) + ((xP % 8) / 4);
}
static inline int derive_4x4_luma_blk(int xP, int yP) {
    return  xP/4 + yP;
}

static Coord inverse_4x4_luma_blk_scan(int luma4x4BlkIdx) {
    return (Coord) {
        _inverse_raster_scan(luma4x4BlkIdx, 4, 4, 16, 0),
        _inverse_raster_scan(luma4x4BlkIdx, 4, 4, 16, 1)
    };
}

static Coord inverse_4x4_chroma_blk_scan(int chroma4x4BlkIdx) {
    return (Coord) {
        _inverse_raster_scan(chroma4x4BlkIdx, 4, 4, 8, 0),
        _inverse_raster_scan(chroma4x4BlkIdx, 4, 4, 8, 1)
    };
}

static Coord inverse_mb_scan(uint32_t mbAddr, SPS *sps) {
    return (Coord){
        _inverse_raster_scan(
            sps->mb_aff_flag ? mbAddr / 2 : mbAddr,
            16, sps->mb_aff_flag ? 32 : 16,
            sps->pic_width_samples_l, 0),

        _inverse_raster_scan(
            sps->mb_aff_flag ? mbAddr / 2 : mbAddr,
            16, sps->mb_aff_flag ? 32 : 16,
            sps->pic_width_samples_l, 1)
    };
}

static Coord inverse_mb_part_scan(uint8_t mbPartIdx, uint8_t mbPartWidth, uint8_t mbPartHeight) {
    return (Coord) {
        _inverse_raster_scan(
            mbPartIdx,
            mbPartWidth, mbPartHeight,
            16, 0),

        _inverse_raster_scan(
            mbPartIdx,
            mbPartWidth, mbPartHeight,
            16, 1)
        };
}

static Coord inverse_sub_mb_part_scan(int32_t mb_type, uint8_t mbPartIdx, uint8_t subMbPartIdx, uint8_t subMbPartWidth, uint8_t subMpPartHeight) {
    return (Coord) {
        (mb_type == 3 || mb_type == 4 || mb_type == 22)
            ? _inverse_raster_scan(subMbPartIdx, subMbPartWidth, subMpPartHeight, 8, 0)
            : _inverse_raster_scan(subMbPartIdx, 4, 4, 8, 0),

        (mb_type == 3 || mb_type == 4 || mb_type == 22)
            ? _inverse_raster_scan(subMbPartIdx, subMbPartWidth, subMpPartHeight, 8, 1)
            : _inverse_raster_scan(subMbPartIdx, 4, 4, 8, 1)
        };
}

#endif //TOY_H264_MB_H