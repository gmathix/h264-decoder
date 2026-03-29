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
    uint32_t type;
    uint8_t pred_mode;
    int cbp_chroma;
    int cbp_luma;
} I_MbInfo ;

extern const I_MbInfo i_mb_type_info[26];


typedef struct P_MbInfo {
    uint32_t type;
    uint8_t  part_count;
    uint8_t  mb_part_width;
    uint8_t  mb_part_height;
} P_MbInfo ;

extern const P_MbInfo p_mb_type_info[5];
extern const P_MbInfo p_sub_mb_type_info[4];


typedef struct B_MbInfo {
    uint32_t type;
    uint8_t part_count;
    uint8_t  mb_part_width;
    uint8_t  mb_part_height;
} B_MbInfo ;

extern const B_MbInfo b_mb_type_info[23];
extern const B_MbInfo b_sub_mb_type_info[13];



void  decode_macroblock        (SliceHeader *s_h, NalUnit *nal_unit, BitReader *br, ParamSets *ps);
void  mb_pred                  (int type, SliceHeader *s_h, NalUnit *nal_unit, BitReader *br, ParamSets *ps);
void  residual                 (int startIdx, int endIdx, SliceHeader *s_h, BitReader *br, ParamSets *ps)

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