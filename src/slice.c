//
// Created by gmathix on 3/20/26.
//

#include "util/expgolomb.h"
#include "slice.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* 7.3.3 */
void decode_slice_header(NalUnit *nal_unit, BitReader *br, ParamSets *ps) {
    SliceHeader *s_h = calloc(1, sizeof(SliceHeader));


    s_h->first_mb   = read_ue(br);
    s_h->slice_type = read_ue(br);
    s_h->pps_id     = read_ue(br);


    bool i_slice  = IS_I_SLICE(s_h->slice_type);
    bool p_slice  = IS_P_SLICE(s_h->slice_type);
    bool b_slice  = IS_B_SLICE(s_h->slice_type);
    bool sp_slice = IS_SP_SLICE(s_h->slice_type);
    bool si_slice = IS_SI_SLICE(s_h->slice_type);

    PPS *pps = ps->pps_list[s_h->pps_id];
    SPS *sps = ps->sps_list[pps->sps_id];




    // skip color_plane_id for now

    s_h->frame_num = read_u(br, (int32_t)(sps->log2_max_frame_num));


    if (!sps->frame_mbs_only_flag) {
        s_h->field_pic_flag = read_u(br, 1);
        if (s_h->field_pic_flag) {
            s_h->bottom_field_flag = read_u(br, 1);
        }
    }

    s_h->idr_pic_flag = nal_unit->type == NAL_CODED_SLICE_OF_IDR_PICTURE ? 1 : 0;
    if (s_h->idr_pic_flag) {
        s_h->idr_pic_id = read_ue(br);
    }

    if (sps->pic_order_cnt_type == 0) {
        s_h->pic_order_cnt_lsb = read_u(br, sps->log2_max_pic_order_cnt_lsb);

        if (pps->bottom_field_pic_order_in_frame_present_flag ) {
            s_h->delta_pic_order_cnt_bottom = read_se(br);
        }
    }

    if (sps->pic_order_cnt_type == 1 && !sps->delta_pic_order_always_zero_flag) {
        s_h->delta_pic_order_cnt[0] = read_se(br);
        if (pps->bottom_field_pic_order_in_frame_present_flag && !s_h->field_pic_flag) {
            s_h->delta_pic_order_cnt[1] = read_se(br);
        }
    }

    if (pps->redundant_pic_cnt_present_flag) {
        s_h->redundant_pic_cnt = read_ue(br);
    }

    if (b_slice) {
        s_h->direct_spatial_mv_pred_flag = read_u(br, 1);
    }

    if (p_slice || sp_slice || b_slice) {
        s_h->num_ref_idx_active_override_flag = read_u(br, 1);
        if (s_h->num_ref_idx_active_override_flag) {
            s_h->num_ref_idx_l0_active_minus1 = read_ue(br);
            if (b_slice) {
                s_h->num_ref_idx_l1_active_minus1 = read_ue(br);
            }
        }
    }
    if (nal_unit->type == NAL_CODED_SLICE_EXTENSION) {
        /* ref_pic_list_mvc_modification() */
    } else {
        ref_pic_list_modification(nal_unit->type, br);
    }


    if ((pps->weighted_pred_flag && (p_slice || sp_slice)) ||
        (pps->weighted_bipred_idc && b_slice)) {

        pred_weight_table(nal_unit->type, br, sps, pps);
    }

    if (nal_unit->ref_idc != 0) {
        dec_ref_pic_marking(s_h->idr_pic_flag, br, sps, pps);
    }

    if (pps->entropy_coding_mode_flag && !i_slice && !si_slice) {
        /* cabac_init_idc() */
    }

    s_h->slice_qp_delta = read_se(br);

    if (sp_slice || si_slice) {
        if (sp_slice) {
            s_h->sp_for_switch_flag = read_u(br, 1);
        }
        s_h->slice_qs_delta = read_se(br);
    }

    if (pps->deblocking_filter_control_present_flag) {
        s_h->disable_deblocking_filter_idc = read_ue(br);
        if (s_h->disable_deblocking_filter_idc != 1) {
            s_h->slice_alpha_c0_offset_div2 = read_se(br);
            s_h->slice_beta_offset_div2     = read_se(br);
        }
    }
}


/* 7.3.4 */
void decode_slice_data(SliceHeader slice_header, NalUnit *nal_unit, BitReader *br, ParamSets *ps) {

}




/* 7.3.3.1 */
void ref_pic_list_modification(uint8_t type, BitReader *br) {
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
void pred_weight_table(uint8_t type, BitReader *br, SPS *sps, PPS *pps) {
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
void dec_ref_pic_marking(int idr_pic_flag, BitReader *br, SPS *sps, PPS *pps) {
    if (idr_pic_flag) {
        int no_output_of_prior_pics_flag = read_u(br, 1);
        int long_term_reference_flag     = read_u(br, 1);
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