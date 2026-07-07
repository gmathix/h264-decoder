//
// Created by gmathix on 4/7/26.
//

#include "motion_info.h"
#include "picture.h"

#include "slice.h"


const Picture EMPTY_PICTURE = {};


Picture *picture_alloc(SliceHeader *sh, CodecContext *ctx) {
    Picture *p  = calloc(1, sizeof(Picture));

    p->frame_num   = sh->frame_num;
	p->widthY      = sh->sps->pic_width_samples_l;
	p->heightY     = sh->sps->pic_height_samples_l;
	p->widthC      = p->widthY / 2;
	p->heightC     = p->heightY / 2;
	p->widthCropY  = p->widthY - sh->sps->crop_right_offset - sh->sps->crop_left_offset;
	p->heightCropY = p->heightY - sh->sps->crop_bottom_offset - sh->sps->crop_top_offset;
	p->widthCropC  = p->widthCropY / 2;
	p->heightCropC = p->heightCropY / 2;
    p->num_mbs     = (int32_t)sh->sps->pic_width_in_mbs * (int32_t)sh->sps->pic_height_in_map_units;

    p->luma        = calloc(p->widthY * p->heightY, 1);
    p->cb          = calloc(p->widthY/2 * (p->heightY/2), 1);
    p->cr          = calloc(p->widthY/2 * (p->heightY/2), 1);

    p->mb_types     = calloc(p->num_mbs, sizeof( int ));
    p->pred_flags   = calloc(p->num_mbs, sizeof( bool [2][4] ));
    p->motion_info  = calloc(p->num_mbs, sizeof( MotionInfo[16] ));


    if (!ctx->mb_metadata_initialized || p->num_mbs != ctx->num_mbs) {
        if (ctx->mb_metadata_initialized) { /* will have to reallocate the buffers, shouldn't happen mid-stream */
            decoder_free_metadata(ctx);
        }
        decoder_alloc_metadata(ctx);
    }


    return p;
}



void picture_reset(Picture *p) {
    memset(&p->luma[0], 0, p->heightY * p->widthY);
    memset(&p->cb[0],   0, p->heightY/2 * p->widthY/2);
    memset(&p->cr[0],   0, p->heightY/2 * p->widthY/2);
}

void picture_free(Picture *p) {
    free(p->luma);
    free(p->cb);
    free(p->cr);
    free(p->sh);

    free(p->mb_types);
    free(p->pred_flags);
    free(p->motion_info);

    free(p);
}

void dump_picture(Picture *p, CodecContext *ctx) {
    int top    = ctx->ps->sps->crop_top_offset;
    int bottom = ctx->ps->sps->crop_bottom_offset;
    int left   = ctx->ps->sps->crop_left_offset;
    int right  = ctx->ps->sps->crop_right_offset;

    for (int i = top; i < p->heightY - bottom; i++) {
        fwrite(&p->luma[i*p->widthY + left],   1, p->widthCropY, ctx->out_file);
    }

    if (!ctx->dump_monochrome) {
        for (int i = top/2; i < p->heightC - bottom/2; i++) {
            fwrite(&p->cb[i*p->widthC + left/2], 1, p->widthCropC, ctx->out_file);
        }
        for (int i = top/2; i < p->heightC - bottom/2; i++) {
            fwrite(&p->cr[i*p->widthC + left/2], 1, p->widthCropC, ctx->out_file);
        }
    }
}