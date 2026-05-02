//
// Created by gmathix on 3/20/26.
//

#include <stdlib.h>

#include "decoder.h"

#include <stdio.h>
#include <sys/mman.h>

#include "annexb.h"
#include "dequant.h"
#include "nal.h"
#include "picture.h"
#include "dpb.h"
#include "tests/profiler.h"

int debugging = false;



CodecContext *decoder_init(const uint8_t *data, size_t size, char *out_path) {
    if (data == NULL) return NULL;

    CodecContext *ctx = calloc(1, sizeof(CodecContext));
    if (!ctx) {
        return NULL;
    }

    ctx->data = data;
    ctx->size = size;

    BitReader *br = malloc(sizeof(BitReader));
    ctx->br = br;
    ctx->global_bit_offset = 0;

    ParamSets *ps = calloc(1, sizeof(ParamSets));
    ctx->ps = ps;


    ctx->current_slice = slice_alloc();
    ctx->dpb = make_dbp(ctx);


    precompute_level_scale_table(ctx, &flat_4x4_16[0]);

    ctx->currMb = calloc(1, sizeof(Macroblock));
    ctx->prevMb = calloc(1, sizeof(Macroblock));



    ctx->prf = malloc(sizeof(Profiler));
    profiler_init(ctx->prf);
    ctx->out_path = out_path;
    ctx->out_file = fopen(ctx->out_path, "wb");
    if (!ctx->out_file) {
        perror("fopen");
        exit(1);
    }
    setvbuf(ctx->out_file, NULL, _IOFBF, 8*1024*1024); // 4mb buffer

    ctx->initialized = true;

    return ctx;
}


void decoder_run(CodecContext *context) {
    if (!context->initialized) return;

    BitReader nal_br = make_br(context->data, context->size);

    while (bitreader_bits_remaining(&nal_br) > 8) {
        NalUnit *nal = next_nal_unit(&nal_br);

        dispatch_nal_unit(nal, context);


        free(nal->data);
        free(nal);
        // if (context->prf->total_frames > 30000) {
        //     break;
        // }
    }
}

void decoder_free_metadata(CodecContext *ctx) {
    free(ctx->mb_types);
    free(ctx->intra8x8_pred_modes);
    free(ctx->intra4x4_pred_modes);
    free(ctx->luma_total_coeffs);
    free(ctx->cb_total_coeffs);
    free(ctx->cr_total_coeffs);
    free(ctx->mvs_l0);
    free(ctx->mvs_l1);
    free(ctx->pred_flag_l0);
    free(ctx->pred_flag_l1);
}

/* caller's job to make sure metadata gets free beforehand */
void decoder_alloc_metadata(CodecContext *ctx) {
    ctx->num_mbs = (int32_t)ctx->ps->sps->pic_width_in_mbs * (int32_t)ctx->ps->sps->pic_height_in_map_units;

    ctx->mb_types            = calloc(ctx->num_mbs, sizeof( int32_t));
    ctx->intra8x8_pred_modes = calloc(ctx->num_mbs, sizeof( uint8_t        [ 4] ));
    ctx->intra4x4_pred_modes = calloc(ctx->num_mbs, sizeof( uint8_t        [16] ));
    ctx->luma_total_coeffs   = calloc(ctx->num_mbs, sizeof( uint8_t        [16] ));
    ctx->cb_total_coeffs     = calloc(ctx->num_mbs, sizeof( uint8_t        [16] ));
    ctx->cr_total_coeffs     = calloc(ctx->num_mbs, sizeof( uint8_t        [16] ));
    ctx->mvs_l0              = calloc(ctx->num_mbs, sizeof( MotionVector   [16] ));
    ctx->mvs_l1              = calloc(ctx->num_mbs, sizeof( MotionVector   [16] ));
    ctx->pred_flag_l0        = calloc(ctx->num_mbs, sizeof( uint8_t        [ 4] ));
    ctx->pred_flag_l1        = calloc(ctx->num_mbs, sizeof( uint8_t        [ 4] ));

    ctx->mb_metadata_initialized = true;
}

void decoder_free(CodecContext *ctx) {
    munmap((void*)ctx->data, ctx->size);
    free(ctx->br);
    free(ctx->prf);

    free(ctx->ps->sps);
    free(ctx->ps->pps);
    free(ctx->ps);

    free(ctx->current_slice);

    free(ctx->mb_types);
    free(ctx->intra8x8_pred_modes);
    free(ctx->intra4x4_pred_modes);
    free(ctx->luma_total_coeffs);
    free(ctx->cb_total_coeffs);
    free(ctx->cr_total_coeffs);

    dpb_free(ctx->dpb);

    fclose(ctx->out_file);

    free(ctx);
}