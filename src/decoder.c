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
        free(ctx);
        return NULL;
    };

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

    ctx->currMb = make_mb(0, ctx);
    ctx->prevMb = make_mb(0, ctx);



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


    // int num_nals = count_nals(context->data, context->size);
    // NalUnit *nal_units = malloc(num_nals * sizeof(NalUnit));
    // fill_nal_units(context->data, context->size, nal_units, num_nals);
    //
    // int total = 0;
    // for (int i =0 ; i < num_nals; i++) {
    //     total += nal_units[i].size;
    // }
    //
    // int i = 0;
    // while (i < num_nals) {
    //
    //     dispatch_nal_unit(&nal_units[i], context);
    //     // if (context->prf->total_frames > 1000) {
    //     //     break;
    //     // }
    //
    //     i++;
    // }
    //
    // for (int i = 0; i < num_nals; i++) {
    //     free(nal_units[i].data);
    // }
    // free(nal_units);
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