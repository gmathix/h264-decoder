//
// Created by gmathix on 3/12/26.
//

#ifndef TOY_H264_PS_H
#define TOY_H264_PS_H

#include <stdint.h>

#include "bitreader.h"



#define MAX_SPS_COUNT     32
#define MAX_PPS_COUNT     256


typedef struct SPS {
    /* only params used for intra + CAVLC for now.
     * no scaling matrix or VUI params shit that we don't need for now */

    uint8_t buf[4096];
    size_t  size;


    uint32_t sps_id;


    // RAW (from bitstream)
    int      profile_idc; // 66=baseline, 77=main, 100=high
    int      chroma_format_idc;
    int      bit_depth_luma_minus8;
    uint32_t bit_depth_chroma_minus8;
    uint32_t log_max_frame_num_minus4;
    uint32_t pic_order_cnt_type;
    uint32_t pic_width_in_mbs_minus1;
    uint32_t pic_height_in_map_units_minus1;
    int      frame_mbs_only_flag;
    int      direct_8x8_inference_flag;
    int      frame_cropping_flag;

    uint32_t crop_left_offset;
    uint32_t crop_right_offset;
    uint32_t crop_top_offset;
    uint32_t crop_bottom_offset;



    // DERIVED
    int      bit_depth_luma;     // bit_depth_luma_minus8 + 8
    uint32_t bit_depth_chroma;   // bit_depth_chroma_minus8 + 8
    uint32_t log2_max_frame_num; // log2_max_frame_num_minus4 + 4
    uint32_t log2_max_pic_order_cnt_lsb; // log2_max_pic_order_cnt_lbs_minus4 + 4
    uint32_t pic_width_in_mbs; // pic_width_in_mbs_minus1 + 1
    uint32_t pic_height_in_map_units; // (pic_height_in_map_units_minus1 + 1) * (2 - frame_mbs_only_flag)


} SPS ;


typedef struct PPS {
    /* same */


    uint8_t buf[4096];
    size_t  size;


    uint32_t pps_id;
    uint32_t sps_id;

    // RAW (from bitstream)
    int      cabac; // entropy_coding_mode_flag
    int      bottom_field_pic_order_in_frame_present_flag;
    uint32_t slice_group_map_type;
    uint32_t top_left[256];
    uint32_t bottom_right[256];
    int      slice_group_change_direction_flag;
    int32_t  slice_group_id[256];

    int      weighted_pref_flag;
    int      weighted_bipred_idc;
    int32_t  chroma_qp_index_offset;
    int      deblocking_filter_control_present_flag;
    int      constrained_intra_pred_flag;
    int      redundant_pic_cnt_present_flag;

    int      transform_8x8_mode_flag;
    int      pic_scaling_matrix_present_flag;
    uint8_t  scaling_matrix4[6][16];
    uint8_t  scaling_matrix8[6][64];
    int32_t  second_chroma_qp_index_offset;


    // DERIVED
    uint32_t num_slice_groups; // num_slice_groups_minus1 + 1
    uint32_t slice_group_change_rate; // slice_group_change_rate_minus1 + 1
    uint32_t pic_size_in_map_units; // pic_size_in_map_units_minus1 + 1
    uint32_t run_length[256]; // run_length_minus1 + 1
    int32_t  pic_init_qp; // pic_init_qp_minus26 + 26
    int32_t  pic_init_qs; // pic_init_qs_minus26 + 26
    uint32_t num_ref_idx_l0_active; // num_ref_idx_l0_default_active_minus1 + 1
    uint32_t num_ref_idx_l1_active; // num_ref_idx_l1_default_active_minus1 + 1

} PPS ;


typedef struct ParamSets {
    SPS *sps_list[MAX_SPS_COUNT];
    PPS *pps_list[MAX_PPS_COUNT];

    /* currently active sps and pps */
    SPS *sps;
    PPS *pps;

} ParamSets ;


int  get_profile(ParamSets *ps);
int  decode_sps(BitReader *br, ParamSets *ps, int ignore_truncation);
int  decode_pps(BitReader *br, ParamSets *ps, int bit_length);
void ps_uninit(ParamSets *ps);


#endif //TOY_H264_PS_H