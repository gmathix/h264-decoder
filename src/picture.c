//
// Created by gmathix on 4/7/26.
//

#include "picture.h"
#include <stdlib.h>



Picture *picture_alloc(int width, int height, int nb_mbs) {
    Picture *f = calloc(1, sizeof(Picture));
    f->width = width;
    f->height = height;
    f->luma = calloc(width * height, 1);
    f->cb = calloc((width/2) * (height/2), 1);
    f->cr = calloc((width/2) * (height/2), 1);
    f->num_mbs = nb_mbs;
    f->mb_array = calloc(f->num_mbs, sizeof(Macroblock));

    f->strideY = f->width;
    f->strideC = f->width / 2;

    f->luma_total_coeffs = calloc(f->num_mbs, sizeof(uint8_t[16]));
    f->cb_total_coeffs   = calloc(f->num_mbs, sizeof(uint8_t[16]));
    f->cr_total_coeffs   = calloc(f->num_mbs, sizeof(uint8_t[16]));

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


void picture_free(Picture *f) {
    free(f->luma);
    free(f->cb);
    free(f->cr);
    free(f->mb_array);
    free(f->luma_total_coeffs);
    free(f->cr_total_coeffs);
    free(f->cb_total_coeffs);
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