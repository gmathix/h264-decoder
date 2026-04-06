//
// Created by gmathix on 3/12/26.
//

#ifndef TOY_H264_PS_H
#define TOY_H264_PS_H

#include <stdint.h>

#include "util/bitreader.h"
#include "util/logger.h"


#define MAX_SPS_COUNT     32
#define MAX_PPS_COUNT     256


typedef struct SPS {
    /* only params used for intra + CAVLC for now.
     * no scaling matrix or VUI params shit that we don't need for now */


    // RAW (from bitstream)


    uint32_t bit_depth_chroma_minus8;
    uint32_t chroma_format_idc;
    uint32_t crop_bottom_offset;
    uint32_t crop_left_offset;
    uint32_t crop_right_offset;
    uint32_t crop_top_offset;
    uint32_t log2_max_frame_num_minus4;
    uint32_t log2_max_pic_order_cnt_lsb_minus4;
    uint32_t num_ref_frames_in_pic_order_cnt_cycle;
    uint32_t pic_order_cnt_type;
    uint32_t pic_width_in_mbs_minus1;
    uint32_t pic_height_in_map_units_minus1;
    uint32_t sps_id;

    int32_t  offset_for_non_ref_pic;
    int32_t *offset_for_ref_frame;
    int32_t  offset_for_top_to_bottom_field;

    int      bit_depth_luma_minus8;
    int      delta_pic_order_always_zero_flag;
    int      direct_8x8_inference_flag;
    int      frame_cropping_flag;
    int      frame_mbs_only_flag;
    int      mb_aff_flag;
    int      profile_idc; // 66=baseline, 77=main, 100=high
    int      separate_color_plane_flag;


    // DERIVED
    int      bit_depth_luma;     // bit_depth_luma_minus8 + 8
    uint32_t bit_depth_chroma;   // bit_depth_chroma_minus8 + 8
    uint32_t log2_max_frame_num; // log2_max_frame_num_minus4 + 4
    uint32_t log2_max_pic_order_cnt_lsb; // log2_max_pic_order_cnt_lbs_minus4 + 4
    uint32_t pic_width_in_mbs; // pic_width_in_mbs_minus1 + 1
    uint32_t pic_height_in_map_units; // (pic_height_in_map_units_minus1 + 1) * (2 - frame_mbs_only_flag)
    uint32_t pic_width_samples_l;
    uint32_t pic_height_samples_l;

} SPS ;


typedef struct PPS {
    // RAW (from bitstream)
    uint32_t num_ref_idx_l0_active_minus1;
    uint32_t num_ref_idx_l1_active_minus1;
    uint32_t num_slice_groups_minus1;
    uint32_t pps_id;
    uint32_t sps_id;

    int32_t  chroma_qp_index_offset;
    int32_t  pic_init_qp_minus26;
    int32_t  pic_init_qs_minus26;

    int      bottom_field_pic_order_in_frame_present_flag;
    int      constrained_intra_pred_flag;
    int      deblocking_filter_control_present_flag;
    int      entropy_coding_mode_flag;
    int      redundant_pic_cnt_present_flag;
    int      transform_8x8_mode_flag;
    int      weighted_pred_flag;
    int      weighted_bipred_idc;


    // DERIVED
    uint32_t num_ref_idx_l0_active;
    uint32_t num_ref_idx_l1_active;
    uint32_t num_slice_groups;
    int32_t  pic_init_qp;
    int32_t  pic_init_qs;

} PPS ;


typedef struct ParamSets {
    SPS *sps_list[MAX_SPS_COUNT];
    PPS *pps_list[MAX_PPS_COUNT];

    /* currently active sps and pps */
    SPS *sps;
    PPS *pps;

} ParamSets ;


int  get_profile (ParamSets *ps);
int  decode_sps  (size_t global_bit_offset, BitReader *br, ParamSets *ps);
int  decode_pps  (size_t global_bit_offset, BitReader *br, ParamSets *ps);

int decode_vui (size_t global_bit_offset, BitReader *br, ParamSets *ps);

void ps_uninit   (ParamSets *ps);


#endif //TOY_H264_PS_H