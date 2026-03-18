//
// Created by gmathix on 3/12/26.
//

#ifndef TOY_H264_PS_H
#define TOY_H264_PS_H

#include <stdint.h>

#include "util/bitreader.h"



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
    uint32_t chroma_format_idc;
    int      bit_depth_luma_minus8;
    uint32_t bit_depth_chroma_minus8;
    uint32_t log2_max_frame_num_minus4;
    uint32_t pic_order_cnt_type;
    uint32_t log2_max_pic_order_cnt_lsb_minus4;
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
    int      entropy_coding_mode_flag;
    uint32_t pic_init_qp_minus26;

    int32_t  chroma_qp_index_offset;
    int      deblocking_filter_control_present_flag;
    int      constrained_intra_pred_flag;
    int      redundant_pic_cnt_present_flag;


    // DERIVED
    uint32_t pic_init_qp_minus_26;


} PPS ;


typedef struct ParamSets {
    SPS *sps_list[MAX_SPS_COUNT];
    PPS *pps_list[MAX_PPS_COUNT];

    /* currently active sps and pps */
    SPS *sps;
    PPS *pps;

} ParamSets ;


int  get_profile(ParamSets *ps);
int  decode_sps(BitReader *br, ParamSets *ps);
int  decode_pps(BitReader *br, ParamSets *ps, int bit_length);
void ps_uninit(ParamSets *ps);


#endif //TOY_H264_PS_H