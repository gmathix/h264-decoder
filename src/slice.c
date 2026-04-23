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

#include "transform.h"
#include "util/mbutil.h"
#include "util/sliceutil.h"


// see fig 6-14
ALWAYS_INLINE void derive_macroblock_neighbors(Macroblock *mb, CodecContext *ctx) {
    int mb_addr = mb->mbAddr;
    int mb_width = ctx->ps->sps->pic_width_in_mbs;

    if (mb_addr % mb_width != 0) {
        mb->has_mb_a = 1;
        mb->p_mb_a = &ctx->current_slice->p_pic->mb_array[mb_addr-1];
    } else { // top left, can't have A neighbor
        mb->has_mb_a = 0;
    }
    if (mb_addr / mb_width >= 1) {
        mb->has_mb_b = 1;
        mb->p_mb_b = &ctx->current_slice->p_pic->mb_array[mb_addr - mb_width];
    } else { // top row, can't have B neighbor
        mb->has_mb_b = 0;
    }
    if (mb_addr / mb_width >= 1 &&
        (mb_addr+1) % mb_width != 0) {
        mb->has_mb_c = 1;
        mb->p_mb_c = &ctx->current_slice->p_pic->mb_array[mb_addr - mb_width + 1];
    } else { // top row or right column, can't have C neighbor
        mb->has_mb_c = 0;
    }
    if (mb_addr / mb_width >= 1 &&
        mb_addr % mb_width != 0) {
        mb->has_mb_d = 1;
        mb->p_mb_d = &ctx->current_slice->p_pic->mb_array[mb_addr - mb_width - 1];
        } else { // top row or left column, can't have D neighbor
            mb->has_mb_d = 0;
        }
}

const int mb_debug = 0;
const int frame_debug = 0;
const int num_frames_before_stop = 100000;

void decode_slice(NalUnit *nal_unit, CodecContext *ctx) {
    profiler_start_frame(ctx->prf);

    SliceHeader *sh = read_slice_header(nal_unit, ctx);
    read_slice_data(sh, nal_unit, ctx);

    Slice *slice = ctx->current_slice;

    int start = slice->sh->first_mb;
    int end = start + slice->num_mbs;

    for (int i = start; i < end; i++) {
        if (i == mb_debug && ctx->prf->total_frames == frame_debug) {
            printf("DEBUGGING : mb %d, frame %d\n", mb_debug, frame_debug);
            debugging = true;
        } else {
            debugging = false;
        }

        profiler_start_mb(ctx->prf);

        ctx->current_slice->decode_macroblock(&slice->p_pic->mb_array[i], slice, ctx);

        profiler_end_mb(ctx->prf);
    }

    dump_picture(ctx->current_pic, ctx);


    if (slice->num_mbs + sh->first_mb == slice->p_pic->num_mbs) {
        /* here we should add the picture to the DPB before freeing it
         * but this is for next version ;) */
        free(sh);
        picture_free(ctx->current_pic);
    }


    profiler_end_frame(ctx->prf);

    printf("done frame %lu\n\n", ctx->prf->total_frames);

    if (ctx->prf->total_frames == num_frames_before_stop) {
        exit(1);
    }
}

/* 7.3.3 */
SliceHeader *read_slice_header(NalUnit *nal_unit, CodecContext *ctx) {
    BitReader *br = ctx->br;
    ParamSets *ps = ctx->ps;


    SliceHeader *sh = calloc(1, sizeof(SliceHeader));


#if NAL_LOG
    print_annexb_line_info(ctx->global_bit_offset, "SH", "first_mb", ctx->br);
#endif
    sh->first_mb   = read_ue(br);
#if NAL_LOG
    print_annexb_line_value((int16_t)sh->first_mb);
#endif

#if NAL_LOG
    print_annexb_line_info(ctx->global_bit_offset, "SH", "slice_type", ctx->br);
#endif
    sh->slice_type = read_ue(br);
#if NAL_LOG
    print_annexb_line_value((int16_t)sh->slice_type);
#endif

#if NAL_LOG
    print_annexb_line_info(ctx->global_bit_offset, "SH", "pps_id", ctx->br);
#endif
    sh->pps_id     = read_ue(br);
#if NAL_LOG
    print_annexb_line_value((int16_t)sh->pps_id);
#endif


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

#if NAL_LOG
    print_annexb_line_info(ctx->global_bit_offset, "SH", "frame_num", ctx->br);
#endif
    sh->frame_num = read_u(br, (int32_t)(sps->log2_max_frame_num));
#if NAL_LOG
    print_annexb_line_value((int16_t)sh->frame_num);
#endif


    if (!sps->frame_mbs_only_flag) {
        sh->field_pic_flag = read_u(br, 1);
        if (sh->field_pic_flag) {
            sh->bottom_field_flag = read_u(br, 1);
        }
    }

    sh->idr_pic_flag = nal_unit->type == NAL_CODED_SLICE_OF_IDR_PICTURE ? 1 : 0;
    if (sh->idr_pic_flag) {
#if NAL_LOG
        print_annexb_line_info(ctx->global_bit_offset, "SH", "idr_pic_id", ctx->br);
#endif
        sh->idr_pic_id = read_ue(br);
#if NAL_LOG
        print_annexb_line_value((int16_t)sh->idr_pic_id);
#endif
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

#if NAL_LOG
    print_annexb_line_info(ctx->global_bit_offset, "SH", "slice_qp_delta", ctx->br);
#endif
    sh->slice_qp_delta = read_se(br);
#if NAL_LOG
    print_annexb_line_value((int16_t)sh->slice_qp_delta);
#endif

    if (sp_slice || si_slice) {
        if (sp_slice) {
            sh->sp_for_switch_flag = read_u(br, 1);
        }
        sh->slice_qs_delta = read_se(br);
    }

    if (pps->deblocking_filter_control_present_flag) {
#if NAL_LOG
        print_annexb_line_info(ctx->global_bit_offset, "SH", "disable_deblocking_filter_idc", ctx->br);
#endif
        sh->disable_deblocking_filter_idc = read_ue(br);
#if NAL_LOG
        print_annexb_line_value((int16_t)sh->disable_deblocking_filter_idc);
#endif

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
        slice_reset(ctx->current_slice);

        ctx->current_pic = picture_alloc(sps->pic_width_samples_l, sps->pic_height_samples_l, (int32_t)sh->sps->pic_width_in_mbs * (int32_t)sh->sps->pic_height_in_map_units);
        ctx->current_slice->sh = sh;
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
        if (mbaff_frame_flag && (currMbAddr%2 == 0 ||
            (currMbAddr%2 == 1 && prevMbSkipped))) {

            int mb_field_decoding_flag = read_u(br, 1);
        }

        Macroblock mb = {0};
        mb.mbAddr = currMbAddr;
        mb.mb_y = currMbAddr / sps->pic_width_in_mbs;
        mb.mb_x = currMbAddr % sps->pic_width_in_mbs;

        derive_macroblock_neighbors(&mb, ctx);

        mb.p_frame = ctx->current_pic;
        ctx->current_pic->mb_array[currMbAddr] = mb;
        read_macroblock(ctx->current_pic->mb_array, currMbAddr, sh, nal_unit, ctx);


        if (!pps->entropy_coding_mode_flag) {
            moreDataFlag = more_rbsp_data(br);
        } else {
            /* cabac shit */
        }

        currMbAddr++;
        ctx->current_slice->num_mbs++;

    } while (currMbAddr < ctx->current_pic->num_mbs);
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
#if NAL_LOG
        print_annexb_line_info(ctx->global_bit_offset, "SH", "no_output_of_prior_pics_flag", ctx->br);
#endif
        int no_output_of_prior_pics_flag  = read_u(br, 1);
#if NAL_LOG
        print_annexb_line_value((int16_t)no_output_of_prior_pics_flag);
#endif

#if NAL_LOG
        print_annexb_line_info(ctx->global_bit_offset, "SH", "long_term_reference_flag", ctx->br);
#endif
        sh->long_term_reference_flag     = read_u(br, 1);
#if NAL_LOG
        print_annexb_line_value((int16_t)sh->long_term_reference_flag);
#endif

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