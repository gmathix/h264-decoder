//
// Created by gmathix on 3/20/26.
//

#include "util/expgolomb.h"
#include "slice.h"

#include <stdio.h>


void decode_slice_header(NalUnit *nal_unit, BitReader *br, ParamSets *ps) {
    uint32_t first_mb   = read_ue(br);
    uint32_t slice_type = read_ue(br);
    uint32_t pps_id     = read_ue(br);

    bool i_slice  = IS_I_SLICE(slice_type);
    bool p_slice  = IS_P_SLICE(slice_type);
    bool b_slice  = IS_B_SLICE(slice_type);
    bool sp_slice = IS_SP_SLICE(slice_type);
    bool si_slice = IS_SI_SLICE(slice_type);

    PPS *pps = ps->pps_list[pps_id];
    SPS *sps = ps->sps_list[pps->sps_id];

    // skip color_plane_id for now

    uint32_t frame_num = read_u(br, (int32_t)(sps->log2_max_frame_num));

    int idr_pic_flag = nal_unit->type == NAL_CODED_SLICE_OF_IDR_PICTURE ? 1 : 0;
    if (idr_pic_flag) {
        uint32_t idr_pic_id = read_ue(br);
    }

    if (sps->pic_order_cnt_type == 0) {
        int32_t pic_order_cnt_lsb = read_u(br, sps->log2_max_pic_order_cnt_lsb);
    }
    if (sps->pic_order_cnt_type == 1) {

    }

    if (pps->redundant_pic_cnt_present_flag) {
        uint32_t redundant_pic_cnt = read_ue(br);
    }

    if (b_slice) {
        int direct_spatial_mv_pred_flag = read_u(br, 1);

    }
    if (p_slice || sp_slice || b_slice) {
        int num_ref_idx_active_override_flag = read_u(br, 1);
        if (num_ref_idx_active_override_flag) {
            uint32_t num_ref_idx_l0_active_minus1 = read_ue(br);
            if (b_slice) {
                uint32_t num_ref_idx_l1_active_minus1 = read_ue(br);
            }
        }
    }
    if (nal_unit->type == NAL_CODED_SLICE_EXTENSION) {
        /* ref_pic_list_mvc_modification() */
    } else {
        /* ref_pic_list_modification() */
    }

    if ((pps->weighted_pred_flag && (p_slice || sp_slice) ||
        pps->weighted_bipred_idc && b_slice)) {
        /* pred_weight_table() */
    }

    if (nal_unit->ref_idc != 0) {
        /* dec_ref_pic_marking() */
    }

    if (pps->entropy_coding_mode_flag && !i_slice && !si_slice) {
        /* cabac_init_idc() */
    }

    int32_t slice_qp_delta = read_se(br);

    if (sp_slice || si_slice) {
        if (sp_slice) {
            int sp_for_switch_flag = read_u(br, 1);
        }
        int32_t slice_qs_delta = read_se(br);
    }

    if (pps->deblocking_filter_control_present_flag) {
        uint32_t disable_deblocking_filter_idc = read_ue(br);
        if (disable_deblocking_filter_idc != 1) {
            int32_t slice_alpha_c0_offset_div2 = read_se(br);
            int32_t slice_beta_offset_div2     = read_se(br);
        }
    }


}