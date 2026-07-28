//
// Created by gmathix on 3/20/26.
//

#ifndef TOY_H264_SLICE_H
#define TOY_H264_SLICE_H


#include "global.h"
#include "mb.h"
#include "ps.h"
#include "util/expgolomb.h"
#include "util/sliceutil.h"
#include "dpb.h"


typedef struct SliceHeader {
    SPS *sps;
    PPS *pps;

    /* position of the MMCO syntax structure in the slice NAL */
    size_t mmco_position_bits; // byte_pos*8 + bit_pos

    uint32_t cabac_init_idc;
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

    int      adaptive_ref_pic_marking_mode_flag;
    int      bottom_field_flag;
    int      direct_spatial_mv_pred_flag;
    int      field_pic_flag;
    int      idr_pic_flag;
    int      long_term_reference_flag;
    int      num_ref_idx_active_override_flag;
    int      no_output_of_prior_pics_flag;
    int      sp_for_switch_flag;




} SliceHeader ;

typedef struct Slice {
    SliceHeader *sh;

    struct Picture *p_pic;
    int num_mbs;

    int picNumL0Pred;
    int picNumL1Pred;
    bool rplm_occured_l0;
    bool rplm_occured_l1;
} Slice ;


static Slice *slice_alloc() {
    Slice *s = calloc(1, sizeof(Slice));
    return s;
}

static void slice_free(Slice *slice) {
    free(slice);
}

static void slice_reset(Slice *slice) {
    slice->num_mbs = 0;
    slice->p_pic = NULL;
    slice->sh = NULL;
    slice->rplm_occured_l0 = false;
    slice->rplm_occured_l1 = false;
}


/* 7.3.3.2 */
static void pred_weight_table(uint8_t type, SliceHeader *sh, CodecContext *ctx) {
    BitReader *br = ctx->br;
    SPS *sps = sh->sps;
    PPS *pps = sh->pps;


    ctx->wpred.luma_log2_weight_denom = read_ue(br);
    if (sps->chroma_format_idc != 0) {
        ctx->wpred.chroma_log2_weight_denom = read_ue(br);
    }

    int luma_weight_l0_flag;
    int chroma_weight_l0_flag;

    for (int i = 0; i < sh->num_ref_idx_l0_active_minus1+1; i++) {
        luma_weight_l0_flag = read_u(br, 1);
        if (luma_weight_l0_flag) {
            ctx->wpred.luma_weight[L0][i] = read_se(br);
            ctx->wpred.luma_offset[L0][i] = read_se(br);
        } else {
            ctx->wpred.luma_weight[L0][i] = 1 << ctx->wpred.luma_log2_weight_denom;
            ctx->wpred.luma_offset[L0][i] = 0;
        }

        if (sps->chroma_format_idc != 0) {
            chroma_weight_l0_flag = read_u(br, 1);
            if (chroma_weight_l0_flag) {
                for (int j = 0; j < 2; j++) {
                    ctx->wpred.chroma_weight[L0][i][j] = read_se(br);
                    ctx->wpred.chroma_offset[L0][i][j] = read_se(br);
                }
            } else {
                for (int j = 0; j < 2; j++) {
                    ctx->wpred.chroma_weight[L0][i][j] = 1 << ctx->wpred.chroma_log2_weight_denom;
                    ctx->wpred.chroma_offset[L0][i][j] = 0;
                }
            }
        }
    }


    int luma_weight_l1_flag;
    int chroma_weight_l1_flag;

    if (type%5 == 1) {
        for (int i = 0; i < sh->num_ref_idx_l1_active_minus1+1; i++) {
            luma_weight_l1_flag = read_u(br, 1);
            if (luma_weight_l1_flag) {
                ctx->wpred.luma_weight[L1][i] = read_se(br);
                ctx->wpred.luma_offset[L1][i] = read_se(br);
            } else {
                ctx->wpred.luma_weight[L1][i] = 1 << ctx->wpred.luma_log2_weight_denom;
                ctx->wpred.luma_offset[L1][i] = 0;
            }

            if (sps->chroma_format_idc != 0) {
                chroma_weight_l1_flag = read_u(br, 1);
                if (chroma_weight_l1_flag) {
                    for (int j = 0; j < 2; j++) {
                        ctx->wpred.chroma_weight[L1][i][j] = read_se(br);
                        ctx->wpred.chroma_offset[L1][i][j] = read_se(br);
                    }
                } else {
                    for (int j = 0; j < 2; j++) {
                        ctx->wpred.chroma_weight[L1][i][j] = 1 << ctx->wpred.chroma_log2_weight_denom;
                        ctx->wpred.chroma_offset[L1][i][j] = 0;
                    }
                }
            }
        }
    }
}


/* 7.3.3 */
static SliceHeader *read_slice_header(NalUnit *nal_unit, CodecContext *ctx) {


    BitReader *br = ctx->br;
    ParamSets *ps = ctx->ps;


    SliceHeader *sh = calloc(1, sizeof(SliceHeader));

    sh->first_mb   = read_ue(br);
    sh->slice_type = read_ue(br);
    sh->pps_id     = read_ue(br);

    printf("DECODING %s SLICE\n", slice_type_to_string(sh->slice_type));
    printf("  first_mb:%d\n", sh->first_mb);


    bool i_slice  = IS_I_SLICE(sh->slice_type);
    bool p_slice  = IS_P_SLICE(sh->slice_type);
    bool b_slice  = IS_B_SLICE(sh->slice_type);
    bool sp_slice = IS_SP_SLICE(sh->slice_type);
    bool si_slice = IS_SI_SLICE(sh->slice_type);


    PPS *pps = ps->pps_list[sh->pps_id];
    SPS *sps = ps->sps_list[pps->sps_id];

    sh->pps = pps;
    sh->sps = sps;
    sh->idr_pic_flag = nal_unit->type == NAL_CODED_SLICE_OF_IDR_PICTURE;





    // skip color_plane_id for now
    sh->frame_num = read_u(br, (int32_t)(sps->log2_max_frame_num));

    /* initialize current slice */
    slice_reset(ctx->current_slice);
    ctx->current_slice->sh = sh;
    ctx->current_slice->picNumL0Pred = sh->frame_num;
    ctx->current_slice->picNumL1Pred = sh->frame_num;




    if (!sps->frame_mbs_only_flag) {
        sh->field_pic_flag = read_u(br, 1);
        if (sh->field_pic_flag) {
            sh->bottom_field_flag = read_u(br, 1);
        }
    }


    if (sh->idr_pic_flag) {
        sh->idr_pic_id = read_ue(br);
    }

    if (sps->poc_type == 0) {
        sh->poc_lsb = read_u(br, sps->log2_max_poc_lsb);
        if (pps->bottom_field_pic_order_in_frame_present_flag ) {
            sh->delta_poc_bottom = read_se(br);
        }
    }

    if (sps->poc_type == 1 && !sps->delta_pic_order_always_zero_flag) {
        sh->delta_poc[0] = read_se(br);
        if (pps->bottom_field_pic_order_in_frame_present_flag && !sh->field_pic_flag) {
            sh->delta_poc[1] = read_se(br);
        }
    }

    if (pps->redundant_pic_cnt_present_flag) {
        sh->redundant_pic_cnt = read_ue(br);
    }


    /* initialize current picture */
    if (sh->first_mb == 0) {
        ctx->curr_pic = pic_pool_get(ctx->pool);
        picture_reset(ctx->curr_pic);
        // ctx->curr_pic = picture_alloc(sh, ctx);
        ctx->curr_pic->sh = sh;
        ctx->curr_pic->nal_ref_idc = nal_unit->ref_idc;
        ctx->current_slice->p_pic = ctx->curr_pic;
        ctx->curr_pic->pic_num = sh->frame_num;
        ctx->curr_pic->frame_num = sh->frame_num;
        derive_poc(ctx->dpb, ctx->curr_pic);
        printf(" POC : %d\n", ctx->curr_pic->poc);
    }


    // init l0 and l1
    if (IS_P_SLICE(sh->slice_type) || IS_B_SLICE(sh->slice_type)) {
        init_ref_pic_lists(ctx->dpb, sh);

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
        } else {
            sh->num_ref_idx_l0_active_minus1 = pps->num_ref_idx_l0_default_active_minus1;
            sh->num_ref_idx_l1_active_minus1 = pps->num_ref_idx_l1_default_active_minus1;
        }
    }






    if (nal_unit->type == NAL_CODED_SLICE_EXTENSION) {
        /* ref_pic_list_mvc_modification() */
    } else {
        ref_pic_list_modification(sh->slice_type, ctx->current_slice, ctx->maxFrameNum, &ctx->maxLongTermFrameIdx, ctx);
    }


    if ((pps->weighted_pred_flag && (p_slice || sp_slice)) ||
        (pps->weighted_bipred_idc == 1 && b_slice)) {

        pred_weight_table(sh->slice_type, sh, ctx);
    }

    if (nal_unit->ref_idc != 0) {
        dec_ref_pic_marking(ctx->dpb, ctx->current_slice, ctx->br);
    }

    if (pps->cabac_flag && !i_slice && !si_slice) {
        sh->cabac_init_idc = read_ue(br);
    }

    sh->slice_qp_delta = read_se(br);

    if (sp_slice || si_slice) {
        if (sp_slice) {
            sh->sp_for_switch_flag = read_u(br, 1);
        }
        sh->slice_qs_delta = read_se(br);
    }

    if (pps->deblocking_filter_control_present_flag) {
        sh->disable_deblocking_filter_idc = read_ue(br);
        if (sh->disable_deblocking_filter_idc != 1) {
            sh->slice_alpha_c0_offset_div2 = read_se(br);
            sh->slice_beta_offset_div2     = read_se(br);
        }
    }

    if (pps->num_slice_groups_minus1 > 0) {

    }


    return sh;
}



void         decode_slice_data         (SliceHeader *sh, NalUnit *nal_unit, CodecContext *ctx);
void         pred_weight_table         (uint8_t type, SliceHeader *sh, CodecContext *ctx);

#endif //TOY_H264_SLICE_H