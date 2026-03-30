//
// Created by gmathix on 3/21/26.
//

#ifndef TOY_H264_MB_H
#define TOY_H264_MB_H

#include <stdint.h>
#include <stdio.h>

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


typedef struct {
    int16_t chromaDC[2][4];     // FIXME: no assumption for 4:2:0 and use [2][8] for 4:2:2
    int16_t chromaAC[2][4][15]; // FIXME: same, [2][8][15] for 4:2:2
} ResidualScratch ;


typedef void (*residual_function)(int16_t[], int, int, int, BitReader*, SliceHeader*);



void  decode_macroblock        (SliceHeader *s_h, NalUnit *nal_unit, BitReader *br, ParamSets *ps);
void  mb_pred                  (int type, int pred_mode, SliceHeader *s_h, NalUnit *nal_unit, BitReader *br, ParamSets *ps);

void  residual                 (int type, int t_8x8_flag, int startIdx, int endIdx, int cbp_luma, int cbp_chroma,
                                    residual_function residual_block,  BitReader *br, SliceHeader *s_h);

void  residual_luma            (int type, int t_8x8_flag, int cbp_luma, int startIdx, int endIdx,
                                    int16_t i16x16[16], int16_t i16x16AC[16][15], int16_t level4x4[16][16], int16_t level8x8[4][64],
                                    residual_function residual_block, BitReader *br, SliceHeader *s_h);

void  residual_block_cavlc     (int16_t coeffLevel[], int startIdx, int endIdx, int maxNumCoeff, BitReader *br, SliceHeader *s_h);
void  residual_block_cabac     (int16_t coeffLevel[], int startIdx, int endIdx, int maxNumCoeff, BitReader *br, SliceHeader *s_h);



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




static inline int inverse_mb_scan_x          (uint32_t mbAddr, SPS *sps) {
    return _inverse_raster_scan(
        sps->mb_aff_flag ? mbAddr / 2 : mbAddr,
        16, sps->mb_aff_flag ? 32 : 16,
        sps->pic_width_samples_l, 0);
}
static inline int inverse_mb_scan_y          (uint32_t mbAddr, SPS *sps) {
    return _inverse_raster_scan(
        sps->mb_aff_flag ? mbAddr / 2 : mbAddr,
        16, sps->mb_aff_flag ? 32 : 16,
        sps->pic_width_samples_l, 1);
}
static inline int inverse_mb_part_scan_x     (uint8_t mbPartIdx, uint8_t mbPartWidth, uint8_t mbPartHeight) {
    return _inverse_raster_scan(
            mbPartIdx,
            mbPartWidth, mbPartHeight,
            16, 0);
}
static inline int inverse_mb_part_scan_y     (uint8_t mbPartIdx, uint8_t mbPartWidth, uint8_t mbPartHeight) {
    return _inverse_raster_scan(
            mbPartIdx,
            mbPartWidth, mbPartHeight,
            16, 1);
}
static inline int inverse_sub_mb_part_scan_x (int32_t mb_type, uint8_t mbPartIdx, uint8_t subMbPartIdx, uint8_t subMbPartWidth, uint8_t subMpPartHeight) {
    return (mb_type == 3 || mb_type == 4 || mb_type == 22)
        ? _inverse_raster_scan(subMbPartIdx, subMbPartWidth, subMpPartHeight, 8, 0)
        : _inverse_raster_scan(subMbPartIdx, 4, 4, 8, 0);
}
static inline int inverse_sub_mb_part_scan_y (int32_t mb_type, uint8_t mbPartIdx, uint8_t subMbPartIdx, uint8_t subMbPartWidth, uint8_t subMpPartHeight) {
    return (mb_type == 3 || mb_type == 4 || mb_type == 22)
        ? _inverse_raster_scan(subMbPartIdx, subMbPartWidth, subMpPartHeight, 8, 1)
        : _inverse_raster_scan(subMbPartIdx, 4, 4, 8, 1);
}


#endif //TOY_H264_MB_H