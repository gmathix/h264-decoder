//
// Created by gmathix on 3/12/26.
//




#include "global.h"
#include "ps.h"
#include "util/expgolomb.h"


#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>

#include "common.h"


#define MIN_LOG2_MAX_FRAME_NUM    4
#define MAX_LOG2_MAX_FRAME_NUM    (12 + MIN_LOG2_MAX_FRAME_NUM)



/* 7.3.2.1.1 */
int decode_sps(size_t global_bit_offset, BitReader *br, ParamSets *ps) {
    SPS *sps = calloc(1, sizeof(SPS));


#if NAL_LOG
    print_annexb_line_info(global_bit_offset, "SPS", "profile_idc", br);
#endif
    int profile_idc = read_u(br, 8);
#if NAL_LOG
    print_annexb_line_value((int16_t)profile_idc);
#endif



    /// TODO: use 6 next bits for constraints
    bitreader_skip_bits(br, 6);


    bitreader_skip_bits(br, 2); // reserved zero bits

#if NAL_LOG
    print_annexb_line_info(global_bit_offset, "SPS", "level_idc", br);
#endif
    int level_idc = read_u(br, 8);
#if NAL_LOG
    print_annexb_line_value((int16_t)level_idc);
#endif


    uint32_t sps_id = read_ue(br);
    if (sps_id > MAX_SPS_COUNT) {
        printf("sps id %u out of range, max is %d", sps_id, MAX_SPS_COUNT);
        return -1;
    }


#if NAL_LOG
    print_annexb_line_info(global_bit_offset, "SPS", "sps_id", br);
#endif

    sps->sps_id = sps_id;
    sps->profile_idc = profile_idc;

#if NAL_LOG
    print_annexb_line_value((int16_t)sps->sps_id);
#endif

#if NAL_LOG
    print_annexb_line_info(global_bit_offset, "SPS", "chroma_format_idc", br);
#endif


    /* not going to handle these for now. focusing on Baseline + Main profiles
     * probably won't even ever work on these */
    if (sps->profile_idc == PROFILE_HIGH ||
        sps->profile_idc == PROFILE_HIGH_10 ||
        sps->profile_idc == PROFILE_HIGH_422 ||
        sps->profile_idc == PROFILE_HIGH_PRED_444 ||
        sps->profile_idc == PROFILE_CAVLC_444  ||
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
#if NAL_LOG
    print_annexb_line_value((int16_t)sps->chroma_format_idc);
#endif







#if NAL_LOG
    print_annexb_line_info(global_bit_offset, "SPS", "log2_max_frame_num_minus4", br);
#endif
    sps->log2_max_frame_num_minus4 =read_ue(br);
    if (sps->log2_max_frame_num_minus4 < MIN_LOG2_MAX_FRAME_NUM - 4 ||
        sps->log2_max_frame_num_minus4 > MAX_LOG2_MAX_FRAME_NUM) {
        printf("log2_max_frame_num_minus4 out of range(0-12): %d\n", sps->log2_max_frame_num_minus4);
        return -1;
    }
#if NAL_LOG
    print_annexb_line_value((int16_t)sps->log2_max_frame_num_minus4);
#endif

#if NAL_LOG
    print_annexb_line_info(global_bit_offset, "SPS", "pic_order_cnt_type", br);
#endif
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
#if NAL_LOG
    print_annexb_line_value((int16_t)sps->pic_order_cnt_type);
#endif



    /* unused for now */
#if NAL_LOG
    print_annexb_line_info(global_bit_offset, "SPS", "num_ref_frames", br);
#endif
    uint32_t num_ref_frames = read_ue(br);
#if NAL_LOG
    print_annexb_line_value((int16_t)num_ref_frames);
#endif

#if NAL_LOG
    print_annexb_line_info(global_bit_offset, "SPS", "gaps_in_frame_num_value_allowed_flag", br);
#endif
    int32_t gaps_in_frame_num_value_allowed_flag = read_u(br, 1);
#if NAL_LOG
    print_annexb_line_value((int16_t)gaps_in_frame_num_value_allowed_flag);
#endif


#if NAL_LOG
    print_annexb_line_info(global_bit_offset, "SPS", "pic_width_in_mbs_minus1", br);
#endif
    sps->pic_width_in_mbs_minus1 = read_ue(br);
#if NAL_LOG
    print_annexb_line_value((int16_t)sps->pic_width_in_mbs_minus1);
#endif


#if NAL_LOG
    print_annexb_line_info(global_bit_offset, "SPS", "pic_height_in_map_units_minus1", br);
#endif
    sps->pic_height_in_map_units_minus1 = read_ue(br);
#if NAL_LOG
    print_annexb_line_value((int16_t)sps->pic_height_in_map_units_minus1);
#endif


#if NAL_LOG
    print_annexb_line_info(global_bit_offset, "SPS", "frame_mbs_only_flag", br);
#endif
    sps->frame_mbs_only_flag = read_u(br, 1);
#if NAL_LOG
    print_annexb_line_value((int16_t)sps->frame_mbs_only_flag);
#endif

    if (!sps->frame_mbs_only_flag) {
#if NAL_LOG
        print_annexb_line_info(global_bit_offset, "SPS", "mb_aff_flag", br);
#endif
        sps->mb_aff_flag = read_u(br, 1);
#if NAL_LOG
        print_annexb_line_value((int16_t)sps->mb_aff_flag);
#endif
    }

#if NAL_LOG
    print_annexb_line_info(global_bit_offset, "SPS", "direct_8x8_inference_flag", br);
#endif
    sps->direct_8x8_inference_flag = read_u(br, 1);
#if NAL_LOG
    print_annexb_line_value((int16_t)sps->direct_8x8_inference_flag);
#endif


#if NAL_LOG
    print_annexb_line_info(global_bit_offset, "SPS", "frame_cropping_flag", br);
#endif
    sps->frame_cropping_flag = read_u(br, 1);
#if NAL_LOG
    print_annexb_line_value((int16_t)sps->frame_cropping_flag);
#endif

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
#if NAL_LOG
    print_annexb_line_info(global_bit_offset, "SPS", "vui_params_present", br);
#endif
    int vui_params_present = read_u(br, 1);
#if NAL_LOG
    print_annexb_line_value((int16_t)vui_params_present);
#endif

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
    sps->pic_width_samples_l        = sps->pic_width_in_mbs * 16 ;
    sps->pic_height_samples_l       = sps->pic_height_in_map_units * 16;



    free(ps->sps_list[sps->sps_id]);
    ps->sps_list[sps->sps_id] = sps;
    ps->sps = sps;

    return 0;
}


/* 7.3.2.2 */
int decode_pps(size_t global_bit_offset, BitReader *br, ParamSets *ps) {
    PPS *pps = calloc(1, sizeof(PPS));



#if NAL_LOG
    print_annexb_line_info(global_bit_offset, "PPS", "pps_id", br);
#endif
    pps->pps_id = read_ue(br);
    if (pps->pps_id > 255) {
        printf("pps id out of range 0-255: %d\n", pps->pps_id);
        return -1;
    }
#if NAL_LOG
    print_annexb_line_value((int16_t)pps->pps_id);
#endif

#if NAL_LOG
    print_annexb_line_info(global_bit_offset, "PPS", "sps_id", br);
#endif
    pps->sps_id = read_ue(br);
    if (pps->sps_id >= MAX_SPS_COUNT || !ps->sps_list[pps->sps_id]) {
        printf("invalid sps referenced : %d\n", pps->sps_id);
        return -1;
    }
#if NAL_LOG
    print_annexb_line_value((int16_t)pps->sps_id);
#endif

#if NAL_LOG
    print_annexb_line_info(global_bit_offset, "PPS", "entropy_coding_mode_flag", br);
#endif
    pps->entropy_coding_mode_flag = read_u(br, 1);
    if (pps->entropy_coding_mode_flag == 1) {
        printf("CABAC not supported for now\n");
    }
#if NAL_LOG
    print_annexb_line_value((int16_t)pps->entropy_coding_mode_flag);
#endif

#if NAL_LOG
    print_annexb_line_info(global_bit_offset, "PPS", "bottom_field_pic_order_in_frame_present_flag", br);
#endif
    pps->bottom_field_pic_order_in_frame_present_flag = read_u(br, 1);
#if NAL_LOG
    print_annexb_line_value((int16_t)pps->bottom_field_pic_order_in_frame_present_flag);
#endif

#if NAL_LOG
    print_annexb_line_info(global_bit_offset, "PPS", "num_slice_groups_minus1", br);
#endif
    pps->num_slice_groups_minus1 = read_ue(br);
#if NAL_LOG
    print_annexb_line_value((int16_t)pps->num_slice_groups_minus1);
#endif
    assert(pps->num_slice_groups_minus1 == 0); // no slice groups for now, nobody uses them anyway


    /* unused for now */
#if NAL_LOG
    print_annexb_line_info(global_bit_offset, "PPS", "num_ref_idx_l0_active_minus1", br);
#endif
    pps->num_ref_idx_l0_active_minus1              = read_ue(br);
#if NAL_LOG
    print_annexb_line_value((int16_t)pps->num_ref_idx_l0_active_minus1);
#endif

#if NAL_LOG
    print_annexb_line_info(global_bit_offset, "PPS", "num_ref_idx_l1_active_minus1", br);
#endif
    pps->num_ref_idx_l1_active_minus1              = read_ue(br);

#if NAL_LOG
    print_annexb_line_value((int16_t)pps->num_ref_idx_l1_active_minus1);
#endif

#if NAL_LOG
    print_annexb_line_info(global_bit_offset, "PPS", "weighted_pred_flag", br);
#endif
    pps->weighted_pred_flag                        = read_u(br, 1);
#if NAL_LOG
    print_annexb_line_value((int16_t)pps->weighted_pred_flag);
#endif

#if NAL_LOG
    print_annexb_line_info(global_bit_offset, "PPS", "weighted_bipred_flag", br);
#endif
    pps->weighted_bipred_idc                       = read_u(br, 2);
#if NAL_LOG
    print_annexb_line_value((int16_t)pps->weighted_bipred_idc);
#endif



#if NAL_LOG
    print_annexb_line_info(global_bit_offset, "PPS", "pic_init_qp_minus26", br);
#endif
    pps->pic_init_qp_minus26                    = read_se(br);
#if NAL_LOG
    print_annexb_line_value((int16_t)pps->pic_init_qp_minus26);
#endif

#if NAL_LOG
    print_annexb_line_info(global_bit_offset, "PPS", "pic_init_qs_minus26", br);
#endif
    pps->pic_init_qs_minus26                    = read_se(br);
#if NAL_LOG
    print_annexb_line_value((int16_t)pps->pic_init_qs_minus26);
#endif

#if NAL_LOG
    print_annexb_line_info(global_bit_offset, "PPS", "chroma_qp_index_offset", br);
#endif
    pps->chroma_qp_index_offset                 = read_se(br);
#if NAL_LOG
    print_annexb_line_value((int16_t)pps->chroma_qp_index_offset);
#endif

#if NAL_LOG
    print_annexb_line_info(global_bit_offset, "PPS", "deblocking_filter_control_present_flag", br);
#endif
    pps->deblocking_filter_control_present_flag = read_u(br, 1);
#if NAL_LOG
    print_annexb_line_value((int16_t)pps->deblocking_filter_control_present_flag);
#endif

#if NAL_LOG
    print_annexb_line_info(global_bit_offset, "PPS", "constrained_intra_pred_flag", br);
#endif
    pps->constrained_intra_pred_flag            = read_u(br, 1);
#if NAL_LOG
    print_annexb_line_value((int16_t)pps->constrained_intra_pred_flag);
#endif

#if NAL_LOG
    print_annexb_line_info(global_bit_offset, "PPS", "redundant_pic_cnt_present_flag", br);
#endif
    pps->redundant_pic_cnt_present_flag         = read_u(br, 1);
#if NAL_LOG
    print_annexb_line_value((int16_t)pps->redundant_pic_cnt_present_flag);
#endif

    if (more_rbsp_data(br)) {
        printf("MORE\n");
#if NAL_LOG
        print_annexb_line_info(global_bit_offset, "PPS", "transform_8x8_mode_flag", br);
#endif
        pps->transform_8x8_mode_flag = read_u(br, 1);
#if NAL_LOG
        print_annexb_line_value((int16_t)pps->transform_8x8_mode_flag);
#endif
    }


    /* derive */
    pps->num_slice_groups      = pps->num_slice_groups_minus1 + 1;
    pps->num_ref_idx_l0_active = pps->num_ref_idx_l0_active_minus1 + 1;
    pps->num_ref_idx_l1_active = pps->num_ref_idx_l1_active_minus1 + 1;
    pps->pic_init_qp           = pps->pic_init_qp_minus26 + 26;
    pps->pic_init_qs           = pps->pic_init_qs_minus26 + 26;


    free(ps->pps_list[pps->pps_id]);
    ps->pps_list[pps->pps_id] = pps;
    ps->pps = pps;

    return 0;
}


/* E.1.1 */
int decode_vui (size_t global_bit_offset, BitReader *br, ParamSets *ps) {
    /* not used for now, just necessary parsing for bitreader alignement with the reference decoder */

#if NAL_LOG
    print_annexb_line_info(global_bit_offset, "VUI", "aspect_ratio_present_flag", br);
#endif
    int aspect_ratio_present_flag = read_u(br, 1);
#if NAL_LOG
    print_annexb_line_value(aspect_ratio_present_flag);
#endif

    if (aspect_ratio_present_flag) {
#if NAL_LOG
        print_annexb_line_info(global_bit_offset, "VUI", "aspect_ratio_idc", br);
#endif
        int aspect_ratio_idc = read_u(br, 8);
#if NAL_LOG
        print_annexb_line_value(aspect_ratio_idc);
#endif
    }

#if NAL_LOG
    print_annexb_line_info(global_bit_offset, "VUI", "overscan_info_present_flag", br);
#endif
    int overscan_info_present_flag = read_u(br, 1);
#if NAL_LOG
    print_annexb_line_value(overscan_info_present_flag);
#endif

    if (overscan_info_present_flag) {
#if NAL_LOG
        print_annexb_line_info(global_bit_offset, "VUI", "overscan_appropriate_flag", br);
#endif
        int overscan_appropriate_flag = read_u(br, 1);
#if NAL_LOG
        print_annexb_line_value(overscan_appropriate_flag);
#endif
    }

#if NAL_LOG
    print_annexb_line_info(global_bit_offset, "VUI", "video_signal_type_present_flag", br);
#endif
    int video_signal_type_present_flag = read_u(br, 1);
#if NAL_LOG
    print_annexb_line_value(video_signal_type_present_flag);
#endif

    if (video_signal_type_present_flag) {
#if NAL_LOG
        print_annexb_line_info(global_bit_offset, "VUI", "video_format", br);
#endif
        int video_format = read_u(br, 3);
#if NAL_LOG
        print_annexb_line_value(video_format);
#endif

#if NAL_LOG
        print_annexb_line_info(global_bit_offset, "VUI", "video_full_range", br);
#endif
        int video_full_range_flag = read_u(br, 1);
#if NAL_LOG
        print_annexb_line_value(video_full_range_flag);
#endif

#if NAL_LOG
        print_annexb_line_info(global_bit_offset, "VUI", "color_description_present_flag", br);
#endif
        int color_description_present_flag = read_u(br, 1);
#if NAL_LOG
        print_annexb_line_value(color_description_present_flag);
#endif

        if (color_description_present_flag) {
#if NAL_LOG
            print_annexb_line_info(global_bit_offset, "VUI", "color_primaries", br);
#endif
            int color_primaries = read_u(br, 8);
#if NAL_LOG
            print_annexb_line_value(color_primaries);
#endif

#if NAL_LOG
            print_annexb_line_info(global_bit_offset, "VUI", "transfer_characteristics", br);
#endif
            int transfer_characteristics = read_u(br, 8);
#if NAL_LOG
            print_annexb_line_value(transfer_characteristics);
#endif

#if NAL_LOG
            print_annexb_line_info(global_bit_offset, "VUI", "matrix_coeffs", br);
#endif
            int matrix_coeffs = read_u(br, 8);
#if NAL_LOG
            print_annexb_line_value(matrix_coeffs);
#endif
        }
    }

#if NAL_LOG
    print_annexb_line_info(global_bit_offset, "VUI", "chroma_loc_info_present_flag", br);
#endif
    int chroma_loc_info_present_flag = read_u(br, 1);
#if NAL_LOG
    print_annexb_line_value(chroma_loc_info_present_flag);
#endif

    if (chroma_loc_info_present_flag) {
#if NAL_LOG
        print_annexb_line_info(global_bit_offset, "VUI", "chroma_sample_loc_type_top_field", br);
#endif
        uint32_t chroma_sample_loc_type_top_field = read_ue(br);
#if NAL_LOG
        print_annexb_line_value(chroma_sample_loc_type_top_field);
#endif

#if NAL_LOG
        print_annexb_line_info(global_bit_offset, "VUI", "chroma_log_type_bottom_field", br);
#endif
        uint32_t chroma_sample_log_tyep_bottom_field = read_ue(br);
#if NAL_LOG
        print_annexb_line_value(chroma_sample_log_tyep_bottom_field);
#endif
    }

#if NAL_LOG
    print_annexb_line_info(global_bit_offset, "VUI", "timing_info_present_flag", br);
#endif
    int timing_info_present_flag = read_u(br, 1);
#if NAL_LOG
    print_annexb_line_value(timing_info_present_flag);
#endif

    if (timing_info_present_flag) {
#if NAL_LOG
        print_annexb_line_info(global_bit_offset, "VUI", "num_units_in_tick", br);
#endif
        uint32_t num_units_in_ticks = read_u(br, 32);
#if NAL_LOG
        print_annexb_line_value((int32_t)num_units_in_ticks);
#endif

#if NAL_LOG
        print_annexb_line_info(global_bit_offset, "VUI", "time_scale", br);
#endif
        uint32_t time_scale = read_u(br, 32);
#if NAL_LOG
        print_annexb_line_value((int32_t)time_scale);
#endif

#if NAL_LOG
        print_annexb_line_info(global_bit_offset, "VUI", "fixed_frame_rate_flag", br);
#endif
        uint32_t fixed_frame_rate_flag = read_u(br, 1);
#if NAL_LOG
        print_annexb_line_value((int32_t)fixed_frame_rate_flag);
#endif
    }

#if NAL_LOG
    print_annexb_line_info(global_bit_offset, "VUI", "nal_hrd_params_present_flag", br);
#endif
    int nal_hrd_parameters_present_flag = read_u(br, 1);
#if NAL_LOG
    print_annexb_line_value(nal_hrd_parameters_present_flag);
#endif

    if (nal_hrd_parameters_present_flag) {
        /* hrd_parameters() */
    }


#if NAL_LOG
    print_annexb_line_info(global_bit_offset, "VUI", "vcl_hrd_params_present_flag", br);
#endif
    int vcl_hrd_parameters_present_flag = read_u(br, 1);
#if NAL_LOG
    print_annexb_line_value(vcl_hrd_parameters_present_flag);
#endif

    if (vcl_hrd_parameters_present_flag) {
        /* hrd_parameters() */
    }


    if (nal_hrd_parameters_present_flag || vcl_hrd_parameters_present_flag) {
#if NAL_LOG
        print_annexb_line_info(global_bit_offset, "VUI", "low_delay_hrd_flag", br);
#endif
        int low_delay_hrd_flag = read_u(br, 1);
#if NAL_LOG
        print_annexb_line_value(low_delay_hrd_flag);
#endif
    }

#if NAL_LOG
    print_annexb_line_info(global_bit_offset, "VUI", "pic_struct_present_flag", br);
#endif
    int pic_struct_present_flag = read_u(br, 1);
#if NAL_LOG
    print_annexb_line_value(pic_struct_present_flag);
#endif

#if NAL_LOG
    print_annexb_line_info(global_bit_offset, "VUI", "bitstream_restriction_flag", br);
#endif
    int bitstream_restriction_flag = read_u(br, 1);
#if NAL_LOG
    print_annexb_line_value(bitstream_restriction_flag);
#endif

    if (bitstream_restriction_flag) {
#if NAL_LOG
        print_annexb_line_info(global_bit_offset, "VUI", "mvs_over_pic_boundaries_flag", br);
#endif
        int mvs_over_pic_boundaries_flag = read_u(br, 1);
#if NAL_LOG
        print_annexb_line_value(mvs_over_pic_boundaries_flag);
#endif

#if NAL_LOG
        print_annexb_line_info(global_bit_offset, "VUI", "max_bytes_per_pic_denom", br);
#endif
        uint32_t max_bytes_per_pic_denom = read_ue(br);
#if NAL_LOG
        print_annexb_line_value((int32_t)max_bytes_per_pic_denom);
#endif

#if NAL_LOG
        print_annexb_line_info(global_bit_offset, "VUI", "max_bits_per_mb_denom", br);
#endif
        uint32_t max_bits_per_mb_denom = read_ue(br);
#if NAL_LOG
        print_annexb_line_value((int32_t)max_bits_per_mb_denom);
#endif

#if NAL_LOG
        print_annexb_line_info(global_bit_offset, "VUI", "log2_max_mb_length_horizontal", br);
#endif
        uint32_t log2_max_mb_length_horizontal = read_ue(br);
#if NAL_LOG
        print_annexb_line_value((int32_t)log2_max_mb_length_horizontal);
#endif

#if NAL_LOG
        print_annexb_line_info(global_bit_offset, "VUI", "log2_max_mv_length_vertical", br);
#endif
        uint32_t log2_max_mv_length_vertical = read_ue(br);
#if NAL_LOG
        print_annexb_line_value((int32_t)log2_max_mv_length_vertical);
#endif

#if NAL_LOG
        print_annexb_line_info(global_bit_offset, "VUI", "max_num_reorder_frames", br);
#endif
        uint32_t max_num_reorder_frames = read_ue(br);
#if NAL_LOG
        print_annexb_line_value((int32_t)max_num_reorder_frames);
#endif

#if NAL_LOG
        print_annexb_line_info(global_bit_offset, "VUI", "max_dec_frame_buffering", br);
#endif
        uint32_t max_dec_frame_buffering = read_ue(br);
#if NAL_LOG
        print_annexb_line_value((int32_t)max_dec_frame_buffering);
#endif
    }
}

