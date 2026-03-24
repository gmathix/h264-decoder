//
// Created by gmathix on 3/20/26.
//

#ifndef TOY_H264_SLICE_H
#define TOY_H264_SLICE_H


#include "util/bitreader.h"
#include "common.h"
#include "ps.h"


#define SLICE_P            0
#define SLICE_B            1
#define SLICE_I            2
#define SLICE_SP           3
#define SLICE_SI           4
#define SLICE_P_BIS        5
#define SLICE_B_BIS        6
#define SLICE_I_BIS        7
#define SLICE_SP_BIS       8
#define SLICE_SI_BIS       9

#define IS_P_SLICE(a)  (((a) == SLICE_P) || ((a) == SLICE_P_BIS))
#define IS_B_SLICE(a)  (((a) == SLICE_B) || ((a) == SLICE_B_BIS))
#define IS_I_SLICE(a)  (((a) == SLICE_I) || ((a) == SLICE_I_BIS))
#define IS_SP_SLICE(a) (((a) == SLICE_SP) || ((a) == SLICE_SP_BIS))
#define IS_SI_SLICE(a) (((a) == SLICE_SI) || ((a) == SLICE_SI_BIS))


typedef struct SliceHeader {
    SPS *sps;
    PPS *pps;


    uint32_t disable_deblocking_filter_idc;
    uint32_t first_mb;
    uint32_t frame_num;
    uint32_t idr_pic_id;
    uint32_t num_ref_idx_l1_active_minus1;
    uint32_t num_ref_idx_l0_active_minus1;
    uint32_t pps_id;
    uint32_t redundant_pic_cnt;
    uint32_t slice_type;

    int32_t  delta_pic_order_cnt[2];
    int32_t  delta_pic_order_cnt_bottom;
    int32_t  pic_order_cnt_lsb;
    int32_t  slice_qp_delta;
    int32_t  slice_qs_delta;
    int32_t  slice_alpha_c0_offset_div2;
    int32_t  slice_beta_offset_div2;

    int      bottom_field_flag;
    int      direct_spatial_mv_pred_flag;
    int      field_pic_flag;
    int      idr_pic_flag;
    int      num_ref_idx_active_override_flag;
    int      sp_for_switch_flag;


} SliceHeader ;



void decode_slice_header       (NalUnit *nal_unit, BitReader *br, ParamSets *ps);
void decode_slice_data         (SliceHeader slice_header, NalUnit *nal_unit, BitReader *br, ParamSets *ps);
void ref_pic_list_modification (uint8_t type, BitReader *br);
void pred_weight_table         (uint8_t type, BitReader *br, SPS *sps, PPS *pps);
void dec_ref_pic_marking       (int idr_pic_flag, BitReader *br, SPS *sps, PPS *pps);

#endif //TOY_H264_SLICE_H