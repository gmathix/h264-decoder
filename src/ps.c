//
// Created by gmathix on 3/12/26.
//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>


#include "ps.h"

#include <assert.h>
#include <limits.h>
#include <math.h>

#include "util/expgolomb.h"




#define MIN_LOG2_MAX_FRAME_NUM    4
#define MAX_LOG2_MAX_FRAME_NUM    (12 + MIN_LOG2_MAX_FRAME_NUM)



/* 7.3.2.1.1 */
int decode_sps(BitReader *br, ParamSets *ps) {
    SPS *sps = calloc(1, sizeof(SPS));

    sps->size = br->size;

    if (sps->size > sizeof(sps->buf)) {
        printf("[SPS] truncating likely oversized SPS \n");
        sps->size = sizeof(sps->buf);
    }
    memcpy(sps->buf, br->data, sps->size);

    if (!bitreader_byte_aligned(br)) {
        printf("[SPS] bitreader not aligned, cannot read sps\n");
        return -1;
    }


    int profile_idc = read_u(br, 8);

    /// TODO: use 6 next bits for constraints
    bitreader_skip_bits(br, 6);


    bitreader_skip_bits(br, 2); // reserved zero bits

    int level_idc = read_u(br, 8);

    uint32_t sps_id = read_ue(br);
    if (sps_id > MAX_SPS_COUNT) {
        printf("sps id %u out of range, max is %d", sps_id, MAX_SPS_COUNT);
        return -1;
    }

    sps->sps_id = sps_id;
    sps->profile_idc = profile_idc;

    if (sps->profile_idc == 100 || // High profile
        sps->profile_idc == 110 || // High10 profile
        sps->profile_idc == 122 || // High422 profile
        sps->profile_idc == 244 || // Cavlc444 profile
        sps->profile_idc == 44  || // Scalable Constrained High profile (SVC)
        sps->profile_idc == 83  || // Scalable High Intra profile (SVC)
        sps->profile_idc == 86  || // Stereo High profile (MVC)
        sps->profile_idc == 118 || // Multiview Depth High profile (MVCD)
        sps->profile_idc == 128)   // old High444 profile
    {
        sps->chroma_format_idc = read_ue(br);
        if (sps->chroma_format_idc == 3) {
            uint32_t separate_color_plane_flag = read_u(br, 1);
            /// TODO: something
        }

        sps->bit_depth_chroma_minus8 = read_ue(br);
        sps->bit_depth_chroma_minus8 = read_ue(br);

        uint32_t transform_bypass = read_u(br, 1);

        uint32_t scaling_matrix_present = read_u(br, 1);
        if (scaling_matrix_present) {
            printf("scaling matrices not supported yet\n");
            return -1;
        }
    } else {
        sps->chroma_format_idc = 1;
        sps->bit_depth_luma = 8;
        sps->bit_depth_chroma = 8;
    }

    sps->log2_max_frame_num_minus4 =read_ue(br);
    if (sps->log2_max_frame_num_minus4 < MIN_LOG2_MAX_FRAME_NUM - 4 ||
        sps->log2_max_frame_num_minus4 > MAX_LOG2_MAX_FRAME_NUM) {
        printf("log2_max_frame_num_minus4 out of range(0-12): %d\n", sps->log2_max_frame_num_minus4);
        return -1;
    }

    sps->pic_order_cnt_type = read_ue(br);
    if (sps->pic_order_cnt_type == 0) {
        sps->log2_max_pic_order_cnt_lsb_minus4 = read_ue(br);
    } else if (sps->pic_order_cnt_type == 1) {
        sps->delta_pic_order_always_zero_flag      = read_u(br, 1);
        sps->offset_for_non_ref_pic                = read_se(br);
        sps->offset_for_top_to_bottom_field        = read_se(br);
        sps->num_ref_frames_in_pic_order_cnt_cycle = read_ue(br);

        sps->offset_for_ref_frame = malloc(sps->num_ref_frames_in_pic_order_cnt_cycle * sizeof(int32_t));
        for (int i = 0; i < sps->num_ref_frames_in_pic_order_cnt_cycle; i++) {
            sps->offset_for_ref_frame[i] = read_se(br);
        }
        /// TODO: use cycles
        printf("poc type not supported : %d\n", sps->pic_order_cnt_type);
        return -1;
    } else if (sps->pic_order_cnt_type != 2) {
        printf("invalid poc type : %d", sps->pic_order_cnt_type);
        return -1;
    }



    /* unused for now */
    uint32_t num_ref_frames = read_ue(br);
    int32_t gaps_in_frame_num_value_allowed_flag = read_u(br, 1);


    sps->pic_width_in_mbs_minus1 = read_ue(br);
    sps->pic_height_in_map_units_minus1 = read_ue(br);

    sps->frame_mbs_only_flag = read_u(br, 1);
    if (!sps->frame_mbs_only_flag) {
        int32_t mb_aff = read_u(br, 1);
    }

    sps->direct_8x8_inference_flag = read_u(br, 1);

    sps->frame_cropping_flag = read_u(br, 1);
    if (sps->frame_cropping_flag) {
        uint32_t crop_left   = read_ue(br);
        uint32_t crop_right  = read_ue(br);
        uint32_t crop_top    = read_ue(br);
        uint32_t crop_bottom = read_ue(br);
        uint32_t width  = 16 * (sps->pic_width_in_mbs_minus1 + 1);
        uint32_t height = 16 * (sps->pic_height_in_map_units_minus1 + 1);

        int vsub = (sps->chroma_format_idc == 1) ? 1 : 0;
        int hsub = (sps->chroma_format_idc == 1 ||
                    sps->chroma_format_idc == 2) ? 1 : 0;
        int step_x = 1 << hsub;
        int step_y = (2 - sps->frame_mbs_only_flag) << vsub;

        if (crop_left   > (unsigned)INT_MAX / 4 / step_x ||
            crop_right  > (unsigned)INT_MAX / 4 / step_x ||
            crop_top    > (unsigned)INT_MAX / 4 / step_y ||
            crop_bottom > (unsigned)INT_MAX / 4 / step_y ||
            (crop_left + crop_right ) * step_x >= width ||
            (crop_top  + crop_bottom) * step_y >= height)
        {
            printf("crop values invalid %d %d %d %d / %d %d\n", crop_left, crop_right, crop_top, crop_bottom, width, height);
            return -1;
        }

        sps->crop_left_offset   = crop_left * step_x;
        sps->crop_right_offset  = crop_right * step_x;
        sps->crop_top_offset    = crop_top * step_y;
        sps->crop_bottom_offset = crop_bottom * step_y;
    } else {
        sps->crop_left_offset   =
        sps->crop_right_offset  =
        sps->crop_top_offset    =
        sps->crop_bottom_offset = 0;
    }


    /* unused for now */
    int vui_params_present = read_u(br, 1);
    if (vui_params_present) {
        // vui params not supported yet so we'll ignore them ;
    }


    // derive params
    sps->bit_depth_luma             = sps->bit_depth_luma_minus8 + 8;
    sps->bit_depth_chroma           = sps->bit_depth_chroma_minus8 + 8;
    sps->log2_max_frame_num         = sps->log2_max_frame_num_minus4 + 4;
    sps->log2_max_pic_order_cnt_lsb = sps->log2_max_pic_order_cnt_lsb_minus4 + 4;
    sps->pic_width_in_mbs           = sps->pic_width_in_mbs_minus1 + 1;
    sps->pic_height_in_map_units    =
        (sps->pic_height_in_map_units_minus1 + 1) * (2 - sps->frame_mbs_only_flag);


    // log debug info
    // printf("sps:%u profile:%d/%d poc:%d poc_lsb:%d mb_width:%d mb_height:%d px_width:%d px_height:%d crop:%u/%u/%u/%u\n",
    //     sps_id, sps->profile_idc, level_idc,
    //     sps->pic_order_cnt_type, sps->log2_max_pic_order_cnt_lsb,
    //     sps->pic_width_in_mbs, sps->pic_height_in_map_units,
    //     sps->pic_width_in_mbs * 16, sps->pic_height_in_map_units * 16 - sps->crop_bottom_offset,
    //     sps->crop_left_offset, sps->crop_right_offset, sps->crop_top_offset, sps->crop_bottom_offset);


    ps->sps_list[sps->sps_id] = sps;

    return 0;
}


/* 7.3.2.2 */
int decode_pps(BitReader *br, ParamSets *ps) {
    PPS *pps = calloc(1, sizeof(PPS));

    pps->size = br->size;


    if (pps->size > sizeof(pps->buf)) {
        printf("[SPS] truncating likely oversized SPS \n");
        pps->size = sizeof(pps->buf);
    }
    memcpy(pps->buf, br->data, pps->size);

    if (!bitreader_byte_aligned(br)) {
        printf("[SPS] bitreader not aligned, cannot read sps\n");
        return -1;
    }


    pps->pps_id = read_ue(br);
    if (pps->pps_id > 255) {
        printf("pps id out of range 0-255: %d\n", pps->pps_id);
        return -1;
    }
    pps->sps_id = read_ue(br);
    if (pps->sps_id >= MAX_SPS_COUNT || !ps->sps_list[pps->sps_id]) {
        printf("invalid sps referenced : %d\n", pps->sps_id);
        return -1;
    }

    pps->entropy_coding_mode_flag = read_u(br, 1);
    if (pps->entropy_coding_mode_flag == 1) {
        printf("CABAC not supported for now\n");

    }

    pps->bottom_field_pic_order_in_frame_present_flag = read_u(br, 1);

    pps->num_slice_groups_minus1 = read_ue(br);
    assert(pps->num_slice_groups_minus1 == 0); // no slice groups for now, nobody uses them anyway


    /* unused for now */
    pps->num_ref_idx_l0_active_minus1              = read_ue(br);
    pps->num_ref_idx_l1_active_minus1              = read_ue(br);
    pps->weighted_pred_flag                        = read_u(br, 1);
    pps->weighted_bipred_idc                       = read_u(br, 2);



    pps->pic_init_qp_minus26                    = read_se(br);
    pps->pic_init_qs_minus26                    = read_se(br);
    pps->chroma_qp_index_offset                 = read_se(br);

    pps->deblocking_filter_control_present_flag = read_u(br, 1);
    pps->constrained_intra_pred_flag            = read_u(br, 1);
    pps->redundant_pic_cnt_present_flag         = read_u(br, 1);


    /* derive */
    pps->num_slice_groups      = pps->num_slice_groups_minus1 + 1;
    pps->num_ref_idx_l0_active = pps->num_ref_idx_l0_active_minus1 + 1;
    pps->num_ref_idx_l1_active = pps->num_ref_idx_l1_active_minus1 + 1;
    pps->pic_init_qp           = pps->pic_init_qp_minus26 + 26;
    pps->pic_init_qs           = pps->pic_init_qs_minus26 + 26;

    // printf("pps:%u sps:%u %s qp:%d/%d/%d %s %s %s\n",
    //     pps->pps_id, pps->sps_id,
    //     pps->entropy_coding_mode_flag ? "CABAC" : "CAVLC",
    //     pps->pic_init_qp, pps->pic_init_qs, pps->chroma_qp_index_offset,
    //     pps->deblocking_filter_control_present_flag ? "LPAR" : "",
    //     pps->constrained_intra_pred_flag            ? "CONSTR" : "",
    //     pps->redundant_pic_cnt_present_flag         ? "REDU" : "");

    ps->pps_list[pps->pps_id] = pps;

    return 0;
}


