//
// Created by gmathix on 4/7/26.
//

#include "picture.h"
#include <stdlib.h>



Picture *picture_alloc(SliceHeader *sh, CodecContext *ctx) {
    Picture *f  = calloc(1, sizeof(Picture));

    f->is_idr                      = sh->is_idr_pic;
    f->long_term_ref               = sh->long_term_reference_flag;
    f->num_ref_idx_active_override = sh->num_ref_idx_active_override_flag;
    f->frame_num                   = sh->frame_num;

    f->width    = sh->sps->pic_width_samples_l;
    f->height   = sh->sps->pic_height_samples_l;
    f->num_mbs  = (int32_t)sh->sps->pic_width_in_mbs * (int32_t)sh->sps->pic_height_in_map_units;
    f->luma     = calloc(f->width * f->height, 1);
    f->cb       = calloc(f->width/2 * (f->height/2), 1);
    f->cr       = calloc(f->width/2 * (f->height/2), 1);

    f->strideY  = f->width;
    f->strideC  = f->width / 2;


    if (!ctx->mb_metadata_initialized || f->num_mbs != ctx->num_mbs) {
        if (ctx->mb_metadata_initialized) { /* will have to reallocate the buffers, shouldn't happen mid-stream */
            free(ctx->mb_types);
            free(ctx->intra8x8_pred_modes);
            free(ctx->intra4x4_pred_modes);
            free(ctx->luma_total_coeffs);
            free(ctx->cb_total_coeffs);
            free(ctx->cr_total_coeffs);
        }
        ctx->num_mbs = f->num_mbs;

        ctx->mb_types            = calloc(f->num_mbs, sizeof(int32_t));
        ctx->intra8x8_pred_modes = calloc(f->num_mbs, sizeof(uint8_t[ 4]));
        ctx->intra4x4_pred_modes = calloc(f->num_mbs, sizeof(uint8_t[16]));
        ctx->luma_total_coeffs   = calloc(f->num_mbs, sizeof(uint8_t[16]));
        ctx->cb_total_coeffs     = calloc(f->num_mbs, sizeof(uint8_t[16]));
        ctx->cr_total_coeffs     = calloc(f->num_mbs, sizeof(uint8_t[16]));

        ctx->mb_metadata_initialized = true;
    }



    return f;
}

Slice *slice_alloc() {
    Slice *s = calloc(1, sizeof(Slice));
    return s;
}

void slice_free(Slice *slice) {
    free(slice);
}

void slice_reset(Slice *slice) {
    slice->num_mbs = 0;
    slice->p_pic = NULL;
    slice->sh = NULL;
}

void picture_reset(Picture *f) {
    memset(&f->luma[0], 0, f->height * f->width);
    memset(&f->cb[0],   0, f->height/2 * f->width/2);
    memset(&f->cr[0],   0, f->height/2 * f->width/2);
}

void picture_free(Picture *f) {
    free(f->luma);
    free(f->cb);
    free(f->cr);
    free(f);
}

void dump_picture(Picture *f, CodecContext *ctx) {
    int start = ctx->ps->sps->crop_top_offset;
    int end   = ctx->ps->sps->pic_height_samples_l - ctx->ps->sps->crop_bottom_offset;
    int left  = ctx->ps->sps->crop_left_offset;
    int right = ctx->ps->sps->pic_width_samples_l  - ctx->ps->sps->crop_right_offset;

    for (int i = start; i < end; i++) {
        fwrite(&f->luma[i*f->strideY + left],   1, right-left, ctx->out_file);
    }

    for (int i = start/2; i < end/2; i++) {
        fwrite(&f->cb[i*f->strideC + left/2], 1, (right-left)/2, ctx->out_file);
    }
    for (int i = start/2; i < end/2; i++) {
        fwrite(&f->cr[i*f->strideC + left/2], 1, (right-left)/2, ctx->out_file);
    }
}