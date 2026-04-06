//
// Created by gmathix on 3/12/26.
//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>


#include "ps.h"

#include <assert.h>
#include <limits.h>
#include <time.h>
#include <unistd.h>

#include "nal.h"
#include "util/expgolomb.h"


#define MIN_LOG2_MAX_FRAME_NUM    4
#define MAX_LOG2_MAX_FRAME_NUM    (12 + MIN_LOG2_MAX_FRAME_NUM)



/* 7.3.2.1.1 */
int decode_sps(size_t global_bit_offset, BitReader *br, ParamSets *ps) {
    SPS *sps = calloc(1, sizeof(SPS));


    if (!bitreader_byte_aligned(br)) {
        printf("[SPS] bitreader not aligned, cannot read sps\n");
        return -1;
    }


    print_annexb_line_info(global_bit_offset, "SPS", "profile_idc", br);
    int profile_idc = read_u(br, 8);
    print_annexb_line_value((int16_t)profile_idc);


    /// TODO: use 6 next bits for constraints
    bitreader_skip_bits(br, 6);


    bitreader_skip_bits(br, 2); // reserved zero bits


    print_annexb_line_info(global_bit_offset, "SPS", "level_idc", br);
    int level_idc = read_u(br, 8);
    print_annexb_line_value((int16_t)level_idc);


    uint32_t sps_id = read_ue(br);
    if (sps_id > MAX_SPS_COUNT) {
        printf("sps id %u out of range, max is %d", sps_id, MAX_SPS_COUNT);
        return -1;
    }



    print_annexb_line_info(global_bit_offset, "SPS", "sps_id", br);

    sps->sps_id = sps_id;
    sps->profile_idc = profile_idc;

    print_annexb_line_value((int16_t)sps->sps_id);


    print_annexb_line_info(global_bit_offset, "SPS", "chroma_format_idc", br);
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
            sps->separate_color_plane_flag = read_u(br, 1);
            /// TODO: something
        }

        sps->bit_depth_luma_minus8 = read_ue(br);
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
    print_annexb_line_value((int16_t)sps->chroma_format_idc);


    print_annexb_line_info(global_bit_offset, "SPS", "log2_max_frame_num_minus4", br);
    sps->log2_max_frame_num_minus4 =read_ue(br);
    if (sps->log2_max_frame_num_minus4 < MIN_LOG2_MAX_FRAME_NUM - 4 ||
        sps->log2_max_frame_num_minus4 > MAX_LOG2_MAX_FRAME_NUM) {
        printf("log2_max_frame_num_minus4 out of range(0-12): %d\n", sps->log2_max_frame_num_minus4);
        return -1;
    }
    print_annexb_line_value((int16_t)sps->log2_max_frame_num_minus4);


    print_annexb_line_info(global_bit_offset, "SPS", "pic_order_cnt_type", br);
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
    print_annexb_line_value((int16_t)sps->pic_order_cnt_type);



    /* unused for now */
    print_annexb_line_info(global_bit_offset, "SPS", "num_ref_frames", br);
    uint32_t num_ref_frames = read_ue(br);
    print_annexb_line_value((int16_t)num_ref_frames);


    print_annexb_line_info(global_bit_offset, "SPS", "gaps_in_frame_num_value_allowed_flag", br);
    int32_t gaps_in_frame_num_value_allowed_flag = read_u(br, 1);
    print_annexb_line_value((int16_t)gaps_in_frame_num_value_allowed_flag);


    print_annexb_line_info(global_bit_offset, "SPS", "pic_width_in_mbs_minus1", br);
    sps->pic_width_in_mbs_minus1 = read_ue(br);
    print_annexb_line_value((int16_t)sps->pic_width_in_mbs_minus1);

    print_annexb_line_info(global_bit_offset, "SPS", "pic_height_in_map_units_minus1", br);
    sps->pic_height_in_map_units_minus1 = read_ue(br);
    print_annexb_line_value((int16_t)sps->pic_height_in_map_units_minus1);


    print_annexb_line_info(global_bit_offset, "SPS", "frame_mbs_only_flag", br);
    sps->frame_mbs_only_flag = read_u(br, 1);
    print_annexb_line_value((int16_t)sps->frame_mbs_only_flag);

    if (!sps->frame_mbs_only_flag) {
        print_annexb_line_info(global_bit_offset, "SPS", "mb_aff_flag", br);
        sps->mb_aff_flag = read_u(br, 1);
        print_annexb_line_value((int16_t)sps->mb_aff_flag);

    }

    print_annexb_line_info(global_bit_offset, "SPS", "direct_8x8_inference_flag", br);
    sps->direct_8x8_inference_flag = read_u(br, 1);
    print_annexb_line_value((int16_t)sps->direct_8x8_inference_flag);


    print_annexb_line_info(global_bit_offset, "SPS", "frame_cropping_flag", br);
    sps->frame_cropping_flag = read_u(br, 1);
    print_annexb_line_value((int16_t)sps->frame_cropping_flag);

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
    print_annexb_line_info(global_bit_offset, "SPS", "vui_params_present", br);
    int vui_params_present = read_u(br, 1);
    print_annexb_line_value((int16_t)vui_params_present);

    if (vui_params_present) {
        decode_vui(global_bit_offset, br, ps);
    }


    // derive params
    sps->bit_depth_luma             = sps->bit_depth_luma_minus8 + 8;
    sps->bit_depth_chroma           = sps->bit_depth_chroma_minus8 + 8;
    sps->log2_max_frame_num         = sps->log2_max_frame_num_minus4 + 4;
    sps->log2_max_pic_order_cnt_lsb = sps->log2_max_pic_order_cnt_lsb_minus4 + 4;
    sps->pic_width_in_mbs           = sps->pic_width_in_mbs_minus1 + 1;
    sps->pic_height_in_map_units    =
        (sps->pic_height_in_map_units_minus1 + 1) * (2 - sps->frame_mbs_only_flag);
    sps->pic_width_samples_l        = sps->pic_width_in_mbs * 16 -
        sps->crop_left_offset - sps->crop_right_offset;
    sps->pic_height_samples_l       = sps->pic_height_in_map_units * 16 -
        sps->crop_top_offset - sps->crop_bottom_offset;



    ps->sps_list[sps->sps_id] = sps;

    return 0;
}


/* 7.3.2.2 */
int decode_pps(size_t global_bit_offset, BitReader *br, ParamSets *ps) {
    PPS *pps = calloc(1, sizeof(PPS));


    if (!bitreader_byte_aligned(br)) {
        printf("[SPS] bitreader not aligned, cannot read sps\n");
        return -1;
    }


    print_annexb_line_info(global_bit_offset, "PPS", "pps_id", br);
    pps->pps_id = read_ue(br);
    if (pps->pps_id > 255) {
        printf("pps id out of range 0-255: %d\n", pps->pps_id);
        return -1;
    }
    print_annexb_line_value((int16_t)pps->pps_id);

    print_annexb_line_info(global_bit_offset, "PPS", "sps_id", br);
    pps->sps_id = read_ue(br);
    if (pps->sps_id >= MAX_SPS_COUNT || !ps->sps_list[pps->sps_id]) {
        printf("invalid sps referenced : %d\n", pps->sps_id);
        return -1;
    }
    print_annexb_line_value((int16_t)pps->sps_id);

    print_annexb_line_info(global_bit_offset, "PPS", "entropy_coding_mode_flag", br);
    pps->entropy_coding_mode_flag = read_u(br, 1);
    if (pps->entropy_coding_mode_flag == 1) {
        printf("CABAC not supported for now\n");
    }
    print_annexb_line_value((int16_t)pps->entropy_coding_mode_flag);

    print_annexb_line_info(global_bit_offset, "PPS", "bottom_field_pic_order_in_frame_present_flag", br);
    pps->bottom_field_pic_order_in_frame_present_flag = read_u(br, 1);
    print_annexb_line_value((int16_t)pps->bottom_field_pic_order_in_frame_present_flag);

    print_annexb_line_info(global_bit_offset, "PPS", "num_slice_groups_minus1", br);
    pps->num_slice_groups_minus1 = read_ue(br);
    print_annexb_line_value((int16_t)pps->num_slice_groups_minus1);
    assert(pps->num_slice_groups_minus1 == 0); // no slice groups for now, nobody uses them anyway


    /* unused for now */
    print_annexb_line_info(global_bit_offset, "PPS", "num_ref_idx_l0_active_minus1", br);
    pps->num_ref_idx_l0_active_minus1              = read_ue(br);
    print_annexb_line_value((int16_t)pps->num_ref_idx_l0_active_minus1);

    print_annexb_line_info(global_bit_offset, "PPS", "num_ref_idx_l1_active_minus1", br);
    pps->num_ref_idx_l1_active_minus1              = read_ue(br);
    print_annexb_line_value((int16_t)pps->num_ref_idx_l1_active_minus1);

    print_annexb_line_info(global_bit_offset, "PPS", "weighted_pred_flag", br);
    pps->weighted_pred_flag                        = read_u(br, 1);
    print_annexb_line_value((int16_t)pps->weighted_pred_flag);

    print_annexb_line_info(global_bit_offset, "PPS", "weighted_bipred_flag", br);
    pps->weighted_bipred_idc                       = read_u(br, 2);
    print_annexb_line_value((int16_t)pps->weighted_bipred_idc);



    print_annexb_line_info(global_bit_offset, "PPS", "pic_init_qp_minus26", br);
    pps->pic_init_qp_minus26                    = read_se(br);
    print_annexb_line_value((int16_t)pps->pic_init_qp_minus26);

    print_annexb_line_info(global_bit_offset, "PPS", "pic_init_qs_minus26", br);
    pps->pic_init_qs_minus26                    = read_se(br);
    print_annexb_line_value((int16_t)pps->pic_init_qs_minus26);

    print_annexb_line_info(global_bit_offset, "PPS", "chroma_qp_index_offset", br);
    pps->chroma_qp_index_offset                 = read_se(br);
    print_annexb_line_value((int16_t)pps->chroma_qp_index_offset);

    print_annexb_line_info(global_bit_offset, "PPS", "deblocking_filter_control_present_flag", br);
    pps->deblocking_filter_control_present_flag = read_u(br, 1);
    print_annexb_line_value((int16_t)pps->deblocking_filter_control_present_flag);

    print_annexb_line_info(global_bit_offset, "PPS", "constrained_intra_pred_flag", br);
    pps->constrained_intra_pred_flag            = read_u(br, 1);
    print_annexb_line_value((int16_t)pps->constrained_intra_pred_flag);

    print_annexb_line_info(global_bit_offset, "PPS", "redundant_pic_cnt_present_flag", br);
    pps->redundant_pic_cnt_present_flag         = read_u(br, 1);
    print_annexb_line_value((int16_t)pps->redundant_pic_cnt_present_flag);

    if (more_rbsp_data(br)) {
        print_annexb_line_info(global_bit_offset, "PPS", "transform_8x8_mode_flag", br);
        pps->transform_8x8_mode_flag = read_u(br, 1);
        print_annexb_line_value((int16_t)pps->transform_8x8_mode_flag);
    }


    /* derive */
    pps->num_slice_groups      = pps->num_slice_groups_minus1 + 1;
    pps->num_ref_idx_l0_active = pps->num_ref_idx_l0_active_minus1 + 1;
    pps->num_ref_idx_l1_active = pps->num_ref_idx_l1_active_minus1 + 1;
    pps->pic_init_qp           = pps->pic_init_qp_minus26 + 26;
    pps->pic_init_qs           = pps->pic_init_qs_minus26 + 26;


    ps->pps_list[pps->pps_id] = pps;

    return 0;
}


/* E.1.1 */
int decode_vui (size_t global_bit_offset, BitReader *br, ParamSets *ps) {
    /* not used for now, just necessary parsing for bitreader alignement with the reference decoder */

    print_annexb_line_info(global_bit_offset, "VUI", "aspect_ratio_present_flag", br);
    int aspect_ratio_present_flag = read_u(br, 1);
    print_annexb_line_value(aspect_ratio_present_flag);

    if (aspect_ratio_present_flag) {
        print_annexb_line_info(global_bit_offset, "VUI", "aspect_ratio_idc", br);
        int aspect_ratio_idc = read_u(br, 8);
        print_annexb_line_value(aspect_ratio_idc);
    }

    print_annexb_line_info(global_bit_offset, "VUI", "overscan_info_present_flag", br);
    int overscan_info_present_flag = read_u(br, 1);
    print_annexb_line_value(overscan_info_present_flag);

    if (overscan_info_present_flag) {
        print_annexb_line_info(global_bit_offset, "VUI", "overscan_appropriate_flag", br);
        int overscan_appropriate_flag = read_u(br, 1);
        print_annexb_line_value(overscan_appropriate_flag);
    }

    print_annexb_line_info(global_bit_offset, "VUI", "video_signal_type_present_flag", br);
    int video_signal_type_present_flag = read_u(br, 1);
    print_annexb_line_value(video_signal_type_present_flag);

    if (video_signal_type_present_flag) {
        print_annexb_line_info(global_bit_offset, "VUI", "video_format", br);
        int video_format = read_u(br, 3);
        print_annexb_line_value(video_format);

        print_annexb_line_info(global_bit_offset, "VUI", "video_full_range", br);
        int video_full_range_flag = read_u(br, 1);
        print_annexb_line_value(video_full_range_flag);

        print_annexb_line_info(global_bit_offset, "VUI", "color_description_present_flag", br);
        int color_description_present_flag = read_u(br, 1);
        print_annexb_line_value(color_description_present_flag);

        if (color_description_present_flag) {
            print_annexb_line_info(global_bit_offset, "VUI", "color_primaries", br);
            int color_primaries = read_u(br, 8);
            print_annexb_line_value(color_primaries);

            print_annexb_line_info(global_bit_offset, "VUI", "transfer_characteristics", br);
            int transfer_characteristics = read_u(br, 8);
            print_annexb_line_value(transfer_characteristics);

            print_annexb_line_info(global_bit_offset, "VUI", "matrix_coeffs", br);
            int matrix_coeffs = read_u(br, 8);
            print_annexb_line_value(matrix_coeffs);
        }
    }

    print_annexb_line_info(global_bit_offset, "VUI", "chroma_loc_info_present_flag", br);
    int chroma_loc_info_present_flag = read_u(br, 1);
    print_annexb_line_value(chroma_loc_info_present_flag);

    if (chroma_loc_info_present_flag) {
        print_annexb_line_info(global_bit_offset, "VUI", "chroma_sample_loc_type_top_field", br);
        uint32_t chroma_sample_loc_type_top_field = read_ue(br);
        print_annexb_line_value(chroma_sample_loc_type_top_field);

        print_annexb_line_info(global_bit_offset, "VUI", "chroma_log_type_bottom_field", br);
        uint32_t chroma_sample_log_tyep_bottom_field = read_ue(br);
        print_annexb_line_value(chroma_sample_log_tyep_bottom_field);
    }


    print_annexb_line_info(global_bit_offset, "VUI", "timing_info_present_flag", br);
    int timing_info_present_flag = read_u(br, 1);
    print_annexb_line_value(timing_info_present_flag);

    if (timing_info_present_flag) {
        print_annexb_line_info(global_bit_offset, "VUI", "num_units_in_tick", br);
        uint32_t num_units_in_ticks = read_u(br, 32);
        print_annexb_line_value((int32_t)num_units_in_ticks);

        print_annexb_line_info(global_bit_offset, "VUI", "time_scale", br);
        uint32_t time_scale = read_u(br, 32);
        print_annexb_line_value((int32_t)time_scale);

        print_annexb_line_info(global_bit_offset, "VUI", "fixed_frame_rate_flag", br);
        uint32_t fixed_frame_rate_flag = read_u(br, 1);
        print_annexb_line_value((int32_t)fixed_frame_rate_flag);
    }


    print_annexb_line_info(global_bit_offset, "VUI", "nal_hrd_params_present_flag", br);
    int nal_hrd_parameters_present_flag = read_u(br, 1);
    print_annexb_line_value(nal_hrd_parameters_present_flag);

    if (nal_hrd_parameters_present_flag) {
        /* hrd_parameters() */
    }


    print_annexb_line_info(global_bit_offset, "VUI", "vcl_hrd_params_present_flag", br);
    int vcl_hrd_parameters_present_flag = read_u(br, 1);
    print_annexb_line_value(vcl_hrd_parameters_present_flag);

    if (vcl_hrd_parameters_present_flag) {
        /* hrd_parameters() */
    }

    if (nal_hrd_parameters_present_flag || vcl_hrd_parameters_present_flag) {
        print_annexb_line_info(global_bit_offset, "VUI", "low_delay_hrd_flag", br);
        int low_delay_hrd_flag = read_u(br, 1);
        print_annexb_line_value(low_delay_hrd_flag);
    }

    print_annexb_line_info(global_bit_offset, "VUI", "pic_struct_present_flag", br);
    int pic_struct_present_flag = read_u(br, 1);
    print_annexb_line_value(pic_struct_present_flag);

    print_annexb_line_info(global_bit_offset, "VUI", "bitstream_restriction_flag", br);
    int bitstream_restriction_flag = read_u(br, 1);
    print_annexb_line_value(bitstream_restriction_flag);

    if (bitstream_restriction_flag) {
        print_annexb_line_info(global_bit_offset, "VUI", "mvs_over_pic_boundaries_flag", br);
        int mvs_over_pic_boundaries_flag = read_u(br, 1);
        print_annexb_line_value(mvs_over_pic_boundaries_flag);

        print_annexb_line_info(global_bit_offset, "VUI", "max_bytes_per_pic_denom", br);
        uint32_t max_bytes_per_pic_denom = read_ue(br);
        print_annexb_line_value((int32_t)max_bytes_per_pic_denom);

        print_annexb_line_info(global_bit_offset, "VUI", "max_bits_per_mb_denom", br);
        uint32_t max_bits_per_mb_denom = read_ue(br);
        print_annexb_line_value((int32_t)max_bits_per_mb_denom);

        print_annexb_line_info(global_bit_offset, "VUI", "log2_max_mb_length_horizontal", br);
        uint32_t log2_max_mb_length_horizontal = read_ue(br);
        print_annexb_line_value((int32_t)log2_max_mb_length_horizontal);

        print_annexb_line_info(global_bit_offset, "VUI", "log2_max_mv_length_vertical", br);
        uint32_t log2_max_mv_length_vertical = read_ue(br);
        print_annexb_line_value((int32_t)log2_max_mv_length_vertical);

        print_annexb_line_info(global_bit_offset, "VUI", "max_num_reorder_frames", br);
        uint32_t max_num_reorder_frames = read_ue(br);
        print_annexb_line_value((int32_t)max_num_reorder_frames);

        print_annexb_line_info(global_bit_offset, "VUI", "max_dec_frame_buffering", br);
        uint32_t max_dec_frame_buffering = read_ue(br);
        print_annexb_line_value((int32_t)max_dec_frame_buffering);
    }
}

