//
// Created by gmathix on 3/20/26.
//

#include "global.h"
#include "picture.h"
#include "intra.h"
#include "slice.h"
#include "util/expgolomb.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dpb.h"
#include "mvpred.h"
#include "transform.h"
#include "tests/profiler.h"
#include "util/mbutil.h"
#include "util/sliceutil.h"


// see fig 6-14
ALWAYS_INLINE void derive_macroblock_neighbors(Macroblock *mb, CodecContext *ctx) {
    int mb_addr = mb->mbAddr;
    int mb_width = ctx->ps->sps->pic_width_in_mbs;

    if (mb_addr % mb_width != 0) {
        mb->has_mb_a = 1;
        mb->mb_a_off = - 1;
    } else { // top left, can't have A neighbor
        mb->has_mb_a = 0;
    }
    if (mb_addr / mb_width >= 1) {
        mb->has_mb_b = 1;
        mb->mb_b_off = - mb_width;
    } else { // top row, can't have B neighbor
        mb->has_mb_b = 0;
    }
    if (mb_addr / mb_width >= 1 &&
        (mb_addr+1) % mb_width != 0) {
        mb->has_mb_c = 1;
        mb->mb_c_off = - mb_width + 1;
    } else { // top row or right column, can't have C neighbor
        mb->has_mb_c = 0;
    }
    if (mb_addr / mb_width >= 1 &&
        mb_addr % mb_width != 0) {
        mb->has_mb_d = 1;
        mb->mb_d_off = - mb_width - 1;
        } else { // top row or left column, can't have D neighbor
            mb->has_mb_d = 0;
        }
}

const int mb_debug = 4976;
const int frame_debug = 6;
const int num_frames_before_stop = 100000;

void decode_slice(NalUnit *nal_unit, CodecContext *ctx) {
    profiler_start_frame(ctx->prf);

    SliceHeader *sh = read_slice_header(nal_unit, ctx);

    read_slice_data(sh, nal_unit, ctx);

    Slice *slice = ctx->current_slice;

    if (slice->num_mbs + sh->first_mb == slice->p_pic->num_mbs) { // end of picture
        store_picture(ctx->dpb, ctx->current_pic);

        // picture_free(ctx->current_pic);
    }


    profiler_end_frame(ctx->prf);

    printf("done frame %lu (frame_num %d)\n\n", ctx->prf->total_frames-1, sh->frame_num);

    if (ctx->prf->total_frames == num_frames_before_stop) {
        exit(1);
    }
}

/* 7.3.3 */
SliceHeader *read_slice_header(NalUnit *nal_unit, CodecContext *ctx) {
    BitReader *br = ctx->br;
    ParamSets *ps = ctx->ps;


    SliceHeader *sh = calloc(1, sizeof(SliceHeader));


    sh->first_mb   = read_ue(br);
    sh->slice_type = read_ue(br);
    sh->pps_id     = read_ue(br);


    bool i_slice  = IS_I_SLICE(sh->slice_type);
    bool p_slice  = IS_P_SLICE(sh->slice_type);
    bool b_slice  = IS_B_SLICE(sh->slice_type);
    bool sp_slice = IS_SP_SLICE(sh->slice_type);
    bool si_slice = IS_SI_SLICE(sh->slice_type);

    PPS *pps = ps->pps_list[sh->pps_id];
    SPS *sps = ps->sps_list[pps->sps_id];

    sh->pps = pps;
    sh->sps = sps;
    sh->is_idr_pic = nal_unit->type == NAL_CODED_SLICE_OF_IDR_PICTURE;






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

    sh->idr_pic_flag = nal_unit->type == NAL_CODED_SLICE_OF_IDR_PICTURE ? 1 : 0;
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
        ref_pic_list_modification(ctx->dpb, sh->slice_type, ctx->current_slice, ctx->maxFrameNum, &ctx->maxLongTermFrameIdx, ctx->br);
    }


    if ((pps->weighted_pred_flag && (p_slice || sp_slice)) ||
        (pps->weighted_bipred_idc && b_slice)) {

        pred_weight_table(nal_unit->type, sh, ctx);
        }

    if (nal_unit->ref_idc != 0) {
        dec_ref_pic_marking(ctx->dpb, ctx->current_slice, ctx->br);
    }

    if (pps->entropy_coding_mode_flag && !i_slice && !si_slice) {
        /* cabac_init_idc() */
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



    if (IS_P_SLICE(sh->slice_type) || IS_B_SLICE(sh->slice_type)) {
        init_ref_pic_lists(ctx->dpb, sh);
    }


    return sh;
}


/* 7.3.4 */
void read_slice_data(SliceHeader *sh, NalUnit *nal_unit, CodecContext *ctx) {
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


    if (currMbAddr == 0) {
        ctx->current_pic = picture_alloc(sh, ctx);
        ctx->current_pic->sh = sh;
        ctx->current_pic->nal_ref_idc = nal_unit->ref_idc;
        ctx->current_slice->p_pic = ctx->current_pic;

        if (IS_I_SLICE(sh->slice_type)) {
            ctx->current_slice->decode_macroblock = &decode_i_macroblock;
        } else if (IS_P_SLICE(sh->slice_type)) {
            ctx->current_slice->decode_macroblock = &decode_p_macroblock;
        } else if (IS_B_SLICE(sh->slice_type)) {
            ctx->current_slice->decode_macroblock = &decode_b_macroblock;
        }
    }

    do {
        if (!IS_I_SLICE(sh->slice_type) && !IS_SI_SLICE(sh->slice_type)) {
            if (!pps->entropy_coding_mode_flag) {
                uint32_t mb_skip_run = read_ue(br);
                prevMbSkipped = mb_skip_run > 0;

                // if (mb_skip_run > 0) {
                //     printf("skipping %d macroblocks\n", mb_skip_run);
                // }

                for (int i = currMbAddr; i < currMbAddr + mb_skip_run; i++) {
                    Macroblock *mb = ctx->currMb;
                    reset_mb(mb, i, ctx);
                    derive_macroblock_neighbors(mb, ctx);

                    mb->mb_type = MB_TYPE_SKIP;
                    mb->slice_type = sh->slice_type;

                    derive_p_skip_mv(mb, ctx);

                    ctx->prevMb = mb;
                }

                currMbAddr += mb_skip_run;
                ctx->current_slice->num_mbs += mb_skip_run;

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

            profiler_start_mb(ctx->prf);

            Macroblock *mb = ctx->currMb;

            reset_mb(mb, currMbAddr, ctx);
            derive_macroblock_neighbors(mb, ctx);

            read_macroblock(mb, sh, nal_unit, ctx);
            ctx->current_slice->decode_macroblock(mb, ctx->current_slice, ctx);

            profiler_end_mb(ctx->prf);
        }


        if (!pps->entropy_coding_mode_flag) {
            moreDataFlag = more_rbsp_data(br);
        } else {
            /* cabac shit */
        }

        if (moreDataFlag) {
            currMbAddr++;
            ctx->current_slice->num_mbs = currMbAddr + 1;
        }
    } while (moreDataFlag);
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
    int32_t luma_weight_l0[sh->num_ref_idx_l0_active_minus1+1];
    int32_t luma_offset_l0[sh->num_ref_idx_l0_active_minus1+1];

    int chroma_weight_l0_flag;
    int32_t chroma_weight_l0[sh->num_ref_idx_l0_active_minus1+1][2];
    int32_t chroma_offset_l0[sh->num_ref_idx_l0_active_minus1+1][2];

    for (int i = 0; i < sh->num_ref_idx_l0_active_minus1+1; i++) {
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
    int32_t luma_weight_l1[sh->num_ref_idx_l1_active_minus1+1];
    int32_t luma_offset_l1[sh->num_ref_idx_l1_active_minus1+1];

    int chroma_weight_l1_flag;
    int32_t chroma_weight_l1[sh->num_ref_idx_l1_active_minus1+1][2];
    int32_t chroma_offset_l1[sh->num_ref_idx_l1_active_minus1+1][2];

    if (type%5 == 1) {
        for (int i = 0; i < sh->num_ref_idx_l1_active_minus1+1; i++) {
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