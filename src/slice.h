//
// Created by gmathix on 3/20/26.
//

#ifndef TOY_H264_SLICE_H
#define TOY_H264_SLICE_H


#include "common.h"
#include "decoder.h"
#include "ps.h"


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

    int32_t  delta_poc[2];
    int32_t  delta_poc_bottom;
    int32_t  poc_lsb;
    int32_t  slice_qp_delta;
    int32_t  slice_qs_delta;
    int32_t  slice_alpha_c0_offset_div2;
    int32_t  slice_beta_offset_div2;

    int      is_idr_pic;
    int      bottom_field_flag;
    int      direct_spatial_mv_pred_flag;
    int      field_pic_flag;
    int      idr_pic_flag;
    int      long_term_reference_flag;
    int      num_ref_idx_active_override_flag;
    int      no_output_of_prior_pics_flag;
    int      sp_for_switch_flag;


} SliceHeader ;



void         decode_slice              (NalUnit *nal_unit, CodecContext *ctx);
SliceHeader  *read_slice_header       (NalUnit *nal_unit, CodecContext *ctx);
void         read_slice_data         (SliceHeader *sh, NalUnit *nal_unit, CodecContext *ctx);
void         pred_weight_table         (uint8_t type, SliceHeader *sh, CodecContext *ctx);

#endif //TOY_H264_SLICE_H