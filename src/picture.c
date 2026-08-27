//
// Created by gmathix on 4/7/26.
//

#include "motion_info.h"
#include "picture.h"

#include "slice.h"


Picture EMPTY_PICTURE = {};


Picture *picture_alloc(SPS *sps, Undo264Context *ctx) {
    Picture *p  = calloc(1, sizeof(Picture));

	p->widthY      = sps->pic_width_samples_l;
	p->heightY     = sps->pic_height_samples_l;
	p->widthC      = p->widthY / 2;
	p->heightC     = p->heightY / 2;
	p->widthCropY  = p->widthY - sps->crop_right_offset - sps->crop_left_offset;
	p->heightCropY = p->heightY - sps->crop_bottom_offset - sps->crop_top_offset;
	p->widthCropC  = p->widthCropY / 2;
	p->heightCropC = p->heightCropY / 2;
    p->num_mbs     = (int32_t)sps->pic_width_in_mbs * (int32_t)sps->pic_height_in_map_units;

    p->luma        = calloc(p->widthY * p->heightY, 1);
    p->cb          = calloc(p->widthY/2 * (p->heightY/2), 1);
    p->cr          = calloc(p->widthY/2 * (p->heightY/2), 1);

    p->mb_types     = calloc(p->num_mbs, sizeof( int ));
    p->pred_flags   = calloc(p->num_mbs, sizeof( bool [2][4] ));
    p->motion_info  = calloc(p->num_mbs, sizeof( MotionInfo[16] ));

    return p;
}



void picture_reset(Picture *p) {
    p->frame_num = 0;
    p->frame_num_offset = 0;
    p->frame_num_wrap = 0;
    p->pic_num = 0;
    p->poc = 0;
    p->dpb_pic_id = 0;
    memset(p->in_list, false, 2);
    memset(p->lowest_list_index, 0, 2);

    p->dpb_status = UNUSED_REF;
    p->long_term_frame_idx = 0;
    p->is_output = false;
    p->top_field_order_cnt = 0;
    p->bottom_field_order_cnt = 0;

}

void picture_free(Picture *p) {
    free(p->luma);
    free(p->cb);
    free(p->cr);

    free(p->mb_types);
    free(p->pred_flags);
    free(p->motion_info);

    free(p);
}

void dump_picture(Picture *p, Undo264Context *ctx) {
    int top    = ctx->ps->sps->crop_top_offset;
    int bottom = ctx->ps->sps->crop_bottom_offset;
    int left   = ctx->ps->sps->crop_left_offset;
    int right  = ctx->ps->sps->crop_right_offset;

    #ifdef DUMP_PICTURES
    #if DUMP_PICTURES
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
    #endif
    #endif
}

void pic_pool_init(PicturePool *pool, Undo264Context *ctx) {
    SPS *sps = ctx->ps->sps;
    int num_mbs = sps->pic_width_in_mbs * sps->pic_height_in_map_units;

    pool->size = 2 * MAX_DPB_SIZE;
    pool->nb_available = pool->size;

    if (!ctx->mb_metadata_initialized || num_mbs != ctx->num_mbs) {
        if (ctx->mb_metadata_initialized) { /* will have to reallocate the buffers, shouldn't happen mid-stream */
            decoder_free_metadata(ctx);
        }
        decoder_alloc_metadata(ctx);
    }

    for (int i = 0; i < pool->size; i++) {
        if (pool->slots[i]) picture_free(pool->slots[i]);
        pool->slots[i] = picture_alloc(sps, ctx);
        if (sps->chroma_format_idc == 0) {
            Picture *pic = pool->slots[i];
            memset(pic->cb, 128u, pic->widthC * pic->heightC);
            memset(pic->cr, 128u, pic->widthC * pic->heightC);
        }

    }
    memset(pool->available, true, pool->size);
    pool->nb_available = pool->size;
}

void pic_pool_free(PicturePool *pool, Undo264Context *ctx) {
    for (int i = 0; i < pool->size; i++) {
        if (pool->slots[i]) picture_free(pool->slots[i]);
    }
    free(pool);
}

Picture *pic_pool_get(PicturePool *pool) {
    for (int i = 0; i < pool->size; i++) {
        if (pool->available[i]) {
            pool->available[i] = false;
            pool->nb_available--;
            return pool->slots[i];
        }
    }
    return NULL; // bug
}

void pic_pool_getback(Picture *pic, PicturePool *pool) {
    int idx = -1;
    for (int i = 0; i < pool->size; i++) {
        if (pool->slots[i] == pic) {
            idx = i;
            break;
        }
    }
    if (idx != -1) {
        pool->available[idx] = true;
        pool->nb_available++;
    }
}