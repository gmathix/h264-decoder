//
// Created by gmathix on 3/20/26.
//

#include "mb.h"
#include "util/expgolomb.h"
#include "slice.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


void decode_slice(NalUnit *nal_unit, CodecContext *ctx) {
    SliceHeader *sh = decode_slice_header(nal_unit, ctx);
    // if (sh->slice_qp_delta == -3) {
    //     return;
    // }
    decode_slice_data(sh, nal_unit, ctx);
}

/* 7.3.3 */
SliceHeader *decode_slice_header(NalUnit *nal_unit, CodecContext *ctx) {
    BitReader *br = ctx->br;
    ParamSets *ps = ctx->ps;


    SliceHeader *sh = calloc(1, sizeof(SliceHeader));


    print_annexb_line_info(ctx->global_bit_offset, "SH", "first_mb", ctx->br);
    sh->first_mb   = read_ue(br);
    print_annexb_line_value((int16_t)sh->first_mb);

    print_annexb_line_info(ctx->global_bit_offset, "SH", "slice_type", ctx->br);
    sh->slice_type = read_ue(br);
    print_annexb_line_value((int16_t)sh->slice_type);

    print_annexb_line_info(ctx->global_bit_offset, "SH", "pps_id", ctx->br);
    sh->pps_id     = read_ue(br);
    print_annexb_line_value((int16_t)sh->pps_id);


    bool i_slice  = IS_I_SLICE(sh->slice_type);
    bool p_slice  = IS_P_SLICE(sh->slice_type);
    bool b_slice  = IS_B_SLICE(sh->slice_type);
    bool sp_slice = IS_SP_SLICE(sh->slice_type);
    bool si_slice = IS_SI_SLICE(sh->slice_type);

    PPS *pps = ps->pps_list[sh->pps_id];
    SPS *sps = ps->sps_list[pps->sps_id];

    sh->pps = pps;
    sh->sps = sps;



    // skip color_plane_id for now

    print_annexb_line_info(ctx->global_bit_offset, "SH", "frame_num", ctx->br);
    sh->frame_num = read_u(br, (int32_t)(sps->log2_max_frame_num));
    print_annexb_line_value((int16_t)sh->frame_num);


    if (!sps->frame_mbs_only_flag) {
        sh->field_pic_flag = read_u(br, 1);
        if (sh->field_pic_flag) {
            sh->bottom_field_flag = read_u(br, 1);
        }
    }

    sh->idr_pic_flag = nal_unit->type == NAL_CODED_SLICE_OF_IDR_PICTURE ? 1 : 0;
    if (sh->idr_pic_flag) {
        print_annexb_line_info(ctx->global_bit_offset, "SH", "idr_pic_id", ctx->br);
        sh->idr_pic_id = read_ue(br);
        print_annexb_line_value((int16_t)sh->idr_pic_id);
    }

    if (sps->pic_order_cnt_type == 0) {
        sh->pic_order_cnt_lsb = read_u(br, sps->log2_max_pic_order_cnt_lsb);

        if (pps->bottom_field_pic_order_in_frame_present_flag ) {
            sh->delta_pic_order_cnt_bottom = read_se(br);
        }
    }

    if (sps->pic_order_cnt_type == 1 && !sps->delta_pic_order_always_zero_flag) {
        sh->delta_pic_order_cnt[0] = read_se(br);
        if (pps->bottom_field_pic_order_in_frame_present_flag && !sh->field_pic_flag) {
            sh->delta_pic_order_cnt[1] = read_se(br);
        }
    }

    if (pps->redundant_pic_cnt_present_flag) {
        sh->redundant_pic_cnt = read_ue(br);
    }

    if (b_slice) {
        sh->direct_spatial_mv_pred_flag = read_u(br, 1);
    }

    if (p_slice || sp_slice || b_slice) {
        sh->num_ref_idx_active_override_flag = read_u(br, 1);
        if (sh->num_ref_idx_active_override_flag) {
            sh->num_ref_idx_l0_active_minus1 = read_ue(br);
            if (b_slice) {
                sh->num_ref_idx_l1_active_minus1 = read_ue(br);
            }
        }
    }
    if (nal_unit->type == NAL_CODED_SLICE_EXTENSION) {
        /* ref_pic_list_mvc_modification() */
    } else {
        ref_pic_list_modification(sh->slice_type, sh, ctx);
    }


    if ((pps->weighted_pred_flag && (p_slice || sp_slice)) ||
        (pps->weighted_bipred_idc && b_slice)) {

        pred_weight_table(nal_unit->type, sh, ctx);
    }

    if (nal_unit->ref_idc != 0) {
        dec_ref_pic_marking(sh, ctx);
    }

    if (pps->entropy_coding_mode_flag && !i_slice && !si_slice) {
        /* cabac_init_idc() */
    }

    print_annexb_line_info(ctx->global_bit_offset, "SH", "slice_qp_delta", ctx->br);
    sh->slice_qp_delta = read_se(br);
    print_annexb_line_value((int16_t)sh->slice_qp_delta);

    if (sp_slice || si_slice) {
        if (sp_slice) {
            sh->sp_for_switch_flag = read_u(br, 1);
        }
        sh->slice_qs_delta = read_se(br);
    }

    if (pps->deblocking_filter_control_present_flag) {
        print_annexb_line_info(ctx->global_bit_offset, "SH", "disable_deblocking_filter_idc", ctx->br);
        sh->disable_deblocking_filter_idc = read_ue(br);
        print_annexb_line_value((int16_t)sh->disable_deblocking_filter_idc);

        if (sh->disable_deblocking_filter_idc != 1) {
            sh->slice_alpha_c0_offset_div2 = read_se(br);
            sh->slice_beta_offset_div2     = read_se(br);
        }
    }

    if (pps->num_slice_groups_minus1 > 0) {

    }

    return sh;
}


/* 7.3.4 */
void decode_slice_data(SliceHeader *sh, NalUnit *nal_unit, CodecContext *ctx) {
    BitReader *br = ctx->br;


    PPS *pps = sh->pps;
    SPS *sps = sh->sps;

    if (pps->entropy_coding_mode_flag) {
        while (!bitreader_byte_aligned(br)) {
            bitreader_skip_bits(br, 1);
        }
    }

    int mbaff_frame_flag = sh->sps->mb_aff_flag;
    int currMbAddr = sh->first_mb * (1 + mbaff_frame_flag);

    int moreDataFlag = 1;
    int prevMbSkipped = 0;

    int size_in_mbs = (int32_t)sh->sps->pic_width_in_mbs * (int32_t)sh->sps->pic_height_in_map_units;
    Macroblock *mb_array = malloc(size_in_mbs * sizeof(Macroblock));


    do {
        if (!IS_I_SLICE(sh->slice_type) && IS_SI_SLICE(sh->slice_type)) {
            if (!pps->entropy_coding_mode_flag) {
                uint32_t mb_skip_run = read_ue(br);
                prevMbSkipped = mb_skip_run > 0;
                printf("%d\n", prevMbSkipped);

                if (mb_skip_run > 0) {
                    moreDataFlag = more_rbsp_data(br);
                }
            } else {
                /* read with CABAC */
            }
        }
        if (moreDataFlag) {
            if (mbaff_frame_flag && (currMbAddr%2 == 0 ||
                (currMbAddr%2 == 1 && prevMbSkipped))) {

                int mb_field_decoding_flag = read_u(br, 1);
                }
            Macroblock mb = {0};
            mb_array[currMbAddr] = mb;
            decode_macroblock(mb_array, currMbAddr, sh, nal_unit, ctx);
        }

        if (!pps->entropy_coding_mode_flag) {
            moreDataFlag = more_rbsp_data(br);
        } else {
            /* cabac shit */
        }

        currMbAddr += 1;
    } while (moreDataFlag && currMbAddr < size_in_mbs);
}




/* 7.3.3.1 */
void ref_pic_list_modification(uint8_t type, SliceHeader *sh, CodecContext *ctx) {
    BitReader *br = ctx->br;


    if (type%5 != 2 && type%5 != 4) {
        int ref_pic_list_modification_flag_l0 = read_u(br, 1);
        if (ref_pic_list_modification_flag_l0) {
            uint32_t modification_of_pic_nums_idc = 0;
            do {
                modification_of_pic_nums_idc = read_ue(br);
                if (modification_of_pic_nums_idc == 0 || modification_of_pic_nums_idc == 1) {
                    uint32_t abs_diff_pic_num_minus1 = read_ue(br);
                } else if (modification_of_pic_nums_idc == 2) {
                    uint32_t long_term_pic_num = read_ue(br);
                }
            } while (modification_of_pic_nums_idc != 3);
        }
    }

    if (type%5 == 1) {
        int ref_pic_list_modification_l1 = read_u(br, 1);
        if (ref_pic_list_modification_l1) {
            uint32_t modification_of_pic_nums_idc = 0;
            do {
                modification_of_pic_nums_idc = read_ue(br);
                if (modification_of_pic_nums_idc == 0 || modification_of_pic_nums_idc == 1) {
                    uint32_t abs_diff_pic_num_minus1 = read_ue(br);
                } else if (modification_of_pic_nums_idc == 2) {
                    uint32_t long_term_pic_num = read_ue(br);
                }
            } while (modification_of_pic_nums_idc != 3);
        }
    }
}


/* 7.3.3.2 */
void pred_weight_table(uint8_t type, SliceHeader *sh, CodecContext *ctx) {
    BitReader *br = ctx->br;
    SPS *sps = sh->sps;
    PPS *pps = sh->pps;


    uint32_t luma_log2_weight_denom = read_ue(br);
    if (sps->chroma_format_idc != 0) {
        uint32_t chroma_log2_weight_denom = read_ue(br);
    }

    int luma_weight_l0_flag;
    int32_t luma_weight_l0[pps->num_ref_idx_l0_active];
    int32_t luma_offset_l0[pps->num_ref_idx_l0_active];

    int chroma_weight_l0_flag;
    int32_t chroma_weight_l0[pps->num_ref_idx_l0_active][2];
    int32_t chroma_offset_l0[pps->num_ref_idx_l0_active][2];

    for (int i = 0; i < pps->num_ref_idx_l0_active; i++) {
        luma_weight_l0_flag = read_u(br, 1);
        if (luma_weight_l0_flag) {
            luma_weight_l0[i] = read_se(br);
            luma_offset_l0[i] = read_se(br);
        }

        if (sps->chroma_format_idc != 0) {
            chroma_weight_l0_flag = read_u(br, 1);
            if (chroma_weight_l0_flag) {
                for (int j = 0; j < 2; j++) {
                    chroma_weight_l0[i][j] = read_se(br);
                    chroma_offset_l0[i][j] = read_se(br);
                }
            }
        }
    }


    int luma_weight_l1_flag;
    int32_t luma_weight_l1[pps->num_ref_idx_l0_active];
    int32_t luma_offset_l1[pps->num_ref_idx_l0_active];

    int chroma_weight_l1_flag;
    int32_t chroma_weight_l1[pps->num_ref_idx_l0_active][2];
    int32_t chroma_offset_l1[pps->num_ref_idx_l0_active][2];

    if (type%5 == 1) {
        for (int i = 0; i < pps->num_ref_idx_l1_active; i++) {
            luma_weight_l1_flag = read_u(br, 1);
            if (luma_weight_l1_flag) {
                luma_weight_l1[i] = read_se(br);
                luma_offset_l1[i] = read_se(br);
            }

            if (sps->chroma_format_idc != 0) {
                chroma_weight_l1_flag = read_u(br, 1);
                if (chroma_weight_l1_flag) {
                    for (int j = 0; j < 2; j++) {
                        chroma_weight_l1[i][j] = read_se(br);
                        chroma_offset_l1[i][j] = read_se(br);
                    }
                }
            }
        }
    }
}


/* 7.3.3.3 */
void dec_ref_pic_marking(SliceHeader *sh, CodecContext *ctx) {
    BitReader *br = ctx->br;

    if (sh->idr_pic_flag) {
        print_annexb_line_info(ctx->global_bit_offset, "SH", "no_output_of_prior_pics_flag", ctx->br);
        int no_output_of_prior_pics_flag  = read_u(br, 1);
        print_annexb_line_value((int16_t)no_output_of_prior_pics_flag);

        print_annexb_line_info(ctx->global_bit_offset, "SH", "long_term_reference_flag", ctx->br);
        sh->long_term_reference_flag     = read_u(br, 1);
        print_annexb_line_value((int16_t)sh->long_term_reference_flag);

    } else {
        int adaptive_ref_pic_marking_mode_flag = read_u(br, 1);
        if (adaptive_ref_pic_marking_mode_flag) {
            uint32_t mem_managenement_control_op = 1;
            do {
                mem_managenement_control_op = read_ue(br);
                if (mem_managenement_control_op == 1 ||
                    mem_managenement_control_op == 3) {
                    uint32_t difference_of_pics_nums_minus1 = read_ue(br);
                }
                if (mem_managenement_control_op == 2) {
                    uint32_t long_term_pic_num = read_ue(br);
                }
                if (mem_managenement_control_op == 3 ||
                    mem_managenement_control_op == 6) {
                    uint32_t max_long_term_frame_idx_plus1 = read_ue(br);
                }
            } while (mem_managenement_control_op != 0);
        }
    }
}