//
// Created by gmathix on 3/20/26.
//


#include <stdio.h>
#include <stdlib.h>

#include "deblock.h"
#include "dpb.h"
#include "intra.h"
#include "picture.h"
#include "slice.h"

#include "tests/profiler.h"

#include "util/expgolomb.h"
#include "util/mbutil.h"
#include "util/sliceutil.h"


// see fig 6-14

void decode_slice(NalUnit *nal_unit, CodecContext *ctx) {
    profiler_start_frame(ctx->prf);

    SliceHeader *sh = read_slice_header(nal_unit, ctx);

    decode_slice_data(sh, nal_unit, ctx);

    Slice *slice = ctx->current_slice;

    if (slice->num_mbs + sh->first_mb == slice->p_pic->num_mbs ||
        slice->num_mbs + sh->first_mb == slice->p_pic->num_mbs+1) { // end of picture

        deblock_picture(ctx->curr_pic, ctx);
        store_picture(ctx->dpb, ctx->curr_pic);

        // picture_free(ctx->current_pic);
    }


    profiler_end_frame(ctx->prf);

    printf("done slice %lu %s(frame_num %d)\n\n",
        ctx->prf->total_frames-1, slice->p_pic->sh->idr_pic_flag ? "(IDR) " : "", sh->frame_num);
}

/* 7.3.3 */
SliceHeader *read_slice_header(NalUnit *nal_unit, CodecContext *ctx) {


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
        ctx->curr_pic = picture_alloc(sh, ctx);
        ctx->curr_pic->sh = sh;
        ctx->curr_pic->nal_ref_idc = nal_unit->ref_idc;
        ctx->current_slice->p_pic = ctx->curr_pic;
        ctx->curr_pic->pic_num = sh->frame_num;
        derive_poc(ctx->dpb, ctx->curr_pic);
        printf(" POC : %d\n", ctx->curr_pic->poc);

        if (IS_I_SLICE(sh->slice_type)) {
            ctx->current_slice->decode_macroblock = &decode_i_macroblock;
        } else if (IS_P_SLICE(sh->slice_type)) {
            ctx->current_slice->decode_macroblock = &decode_p_macroblock;
        } else if (IS_B_SLICE(sh->slice_type)) {
            ctx->current_slice->decode_macroblock = &decode_b_macroblock;
        }
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






    return sh;
}


/* 7.3.4 */
void decode_slice_data(SliceHeader *sh, NalUnit *nal_unit, CodecContext *ctx) {
    BitReader *br = ctx->br;


    PPS *pps = sh->pps;
    SPS *sps = sh->sps;

    if (pps->cabac_flag) {
        while (!bitreader_byte_aligned(br)) {
            bitreader_skip_bits(br, 1);
        }
    }

    int mbaff_frame_flag = sh->sps->mb_aff_flag;
    int currMbAddr = sh->first_mb * (1 + mbaff_frame_flag);

    int moreDataFlag = 1;
    int prevMbSkipped = 0;



    do {
        if (!IS_I_SLICE(sh->slice_type) && !IS_SI_SLICE(sh->slice_type)) {
            if (!pps->cabac_flag) {
                uint32_t mb_skip_run = read_ue(br);

                prevMbSkipped = mb_skip_run > 0;

                // if (ctx->prf->total_frames == 16 && mb_skip_run > 0) {
                //     printf("skip %d mbs\n", mb_skip_run);
                // }

                // decode skipped macroblocks
                for (int i = currMbAddr; i < currMbAddr + mb_skip_run; i++) {
                    Macroblock *mb = ctx->currMb;
                    reset_mb(mb, i, ctx);
                    derive_macroblock_neighbors(mb, ctx);

                    mb->mb_type = MB_TYPE_SKIP;
                    mb->slice_type = sh->slice_type;
                    mb->QPY = mb->mbAddr == sh->first_mb
                        ? _clip3(0, 51, (pps->pic_init_qp + sh->slice_qp_delta + 52) % 52)
                        : ctx->prevMb->QPY;

                    int qPi = _clip3(0, 51, mb->QPY + pps->chroma_qp_index_offset);
                    mb->QPC = QPcTable[qPi];

                    MacroblockMetadata *meta = &ctx->mb_metadata[mb->mbAddr];
                    meta->mb_type    = MB_TYPE_SKIP;
                    meta->QPY        = mb->QPY;
                    meta->QPC        = mb->QPC;
                    meta->cbp_luma   = 0;
                    meta->cbp_chroma = 0;
                    meta->t_8x8_flag = 0;
                    ctx->curr_pic->mb_types[mb->mbAddr] = mb->mb_type;



                    ctx->current_slice->decode_macroblock(mb, ctx->current_slice, ctx);

                    ctx->prevMb = mb;
                }

                currMbAddr += mb_skip_run;
                ctx->current_slice->num_mbs += mb_skip_run;

                if (mb_skip_run > 0) {
                    moreDataFlag = more_rbsp_data(br) && currMbAddr < ctx->num_mbs;
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


        if (!pps->cabac_flag) {
            moreDataFlag = more_rbsp_data(br);
        } else {
            /* cabac shit */
        }

        if (moreDataFlag) {
            currMbAddr++;
            if (currMbAddr >= ctx->num_mbs) {
                moreDataFlag = false;
            } else {
                ctx->current_slice->num_mbs = currMbAddr + 1;
            }
        }
    } while (moreDataFlag);
}






/* 7.3.3.2 */
void pred_weight_table(uint8_t type, SliceHeader *sh, CodecContext *ctx) {
    BitReader *br = ctx->br;
    SPS *sps = sh->sps;
    PPS *pps = sh->pps;


    ctx->luma_log2_weight_denom = read_ue(br);
    if (sps->chroma_format_idc != 0) {
        ctx->chroma_log2_weight_denom = read_ue(br);
    }

    int luma_weight_l0_flag;
    int chroma_weight_l0_flag;

    for (int i = 0; i < sh->num_ref_idx_l0_active_minus1+1; i++) {
        luma_weight_l0_flag = read_u(br, 1);
        if (luma_weight_l0_flag) {
            ctx->luma_weight[L0][i] = read_se(br);
            ctx->luma_offset[L0][i] = read_se(br);
        } else {
            ctx->luma_weight[L0][i] = 1 << ctx->luma_log2_weight_denom;
            ctx->luma_offset[L0][i] = 0;
        }

        if (sps->chroma_format_idc != 0) {
            chroma_weight_l0_flag = read_u(br, 1);
            if (chroma_weight_l0_flag) {
                for (int j = 0; j < 2; j++) {
                    ctx->chroma_weight[L0][i][j] = read_se(br);
                    ctx->chroma_offset[L0][i][j] = read_se(br);
                }
            } else {
                for (int j = 0; j < 2; j++) {
                    ctx->chroma_weight[L0][i][j] = 1 << ctx->chroma_log2_weight_denom;
                    ctx->chroma_offset[L0][i][j] = 0;
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
                ctx->luma_weight[L1][i] = read_se(br);
                ctx->luma_offset[L1][i] = read_se(br);
            } else {
                ctx->luma_weight[L1][i] = 1 << ctx->luma_log2_weight_denom;
                ctx->luma_offset[L1][i] = 0;
            }

            if (sps->chroma_format_idc != 0) {
                chroma_weight_l1_flag = read_u(br, 1);
                if (chroma_weight_l1_flag) {
                    for (int j = 0; j < 2; j++) {
                        ctx->chroma_weight[L1][i][j] = read_se(br);
                        ctx->chroma_offset[L1][i][j] = read_se(br);
                    }
                } else {
                    for (int j = 0; j < 2; j++) {
                        ctx->chroma_weight[L1][i][j] = 1 << ctx->chroma_log2_weight_denom;
                        ctx->chroma_offset[L1][i][j] = 0;
                    }
                }
            }
        }
    }
}