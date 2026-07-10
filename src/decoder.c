//
// Created by gmathix on 3/20/26.
//



#include "decoder.h"

#include "annexb.h"
#include "dequant.h"
#include "dpb.h"
#include "mb.h"
#include "nal.h"
#include "picture.h"
#include "ps.h"
#include "tests/profiler.h"
#include "util/bitreader.h"


int debugging = 0;
int frame_debug = 5;
int frame_num_debug = -1;
int poc_debug = -1;
int mb_debug = 4977;
int nb_frames_before_stop = -1;


CodecContext *decoder_init(const uint8_t *data, size_t size, char *out_path, char *log_path, bool dump_monochrome) {

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


    compute_4x4_scale(ctx, flat_4x4_16);
    compute_8x8_scale(ctx, flat_8x8_16);

    ctx->currMb = calloc(1, sizeof(Macroblock));
    ctx->prevMb = calloc(1, sizeof(Macroblock));



    ctx->prf = malloc(sizeof(Profiler));
    profiler_init(ctx->prf);
    ctx->out_path = out_path;
    ctx->out_file = fopen(ctx->out_path, "wb");
    ctx->dump_monochrome = dump_monochrome;
    if (!ctx->out_file) {
        perror("fopen");
        exit(123454321);
    }
    setvbuf(ctx->out_file, NULL, _IOFBF, (size_t) 8 * 1920*1080*1.5); // 8 frame buffer for HD

    ctx->log_path = log_path;
    ctx->log_file = fopen(ctx->log_path, "w");

    ctx->initialized = true;

    return ctx;
}


void decoder_run(CodecContext *ctx) {
    if (!ctx->initialized) return;

    BitReader nal_br = make_br(ctx->data, ctx->size);

    while (bitreader_bits_remaining(&nal_br) > 8) {
        NalUnit *nal = next_nal_unit(&nal_br);

        dispatch_nal_unit(nal, ctx);


        free(nal->data);
        free(nal);


        if (ctx->prf->total_frames == nb_frames_before_stop) {
            break;
        }
    }

	dpb_flush(ctx->dpb);
	fflush(ctx->out_file);
}

void decoder_free_metadata(CodecContext *ctx) {
    free(ctx->mb_metadata);
    free(ctx->intra8x8_pred_modes);
    free(ctx->intra4x4_pred_modes);
    free(ctx->luma_total_coeffs);
    free(ctx->cb_total_coeffs);
    free(ctx->cr_total_coeffs);
}

/* caller's job to make sure metadata gets free beforehand */
void decoder_alloc_metadata(CodecContext *ctx) {
    printf("allocating metadata : num_mbs %d\n", ctx->num_mbs);
    ctx->num_mbs = (int32_t)ctx->ps->sps->pic_width_in_mbs * (int32_t)ctx->ps->sps->pic_height_in_map_units;

    ctx->mb_metadata = calloc(ctx->num_mbs, sizeof( MacroblockMetadata));
    ctx->intra8x8_pred_modes = calloc(ctx->num_mbs, sizeof( uint8_t        [ 4] ));
    ctx->intra4x4_pred_modes = calloc(ctx->num_mbs, sizeof( uint8_t        [16] ));
    ctx->luma_total_coeffs   = calloc(ctx->num_mbs, sizeof( uint8_t        [16] ));
    ctx->cb_total_coeffs     = calloc(ctx->num_mbs, sizeof( uint8_t        [16] ));
    ctx->cr_total_coeffs     = calloc(ctx->num_mbs, sizeof( uint8_t        [16] ));

    ctx->mb_metadata_initialized = true;
}

void decoder_free(CodecContext *ctx) {
    decoder_free_metadata(ctx);

    munmap((void*)ctx->data, ctx->size);
    free(ctx->br);
    free(ctx->prf);

    free(ctx->ps->sps);
    free(ctx->ps->pps);
    free(ctx->ps);

    free(ctx->current_slice);


    free(ctx->prevMb);


    dpb_free(ctx->dpb);

    fclose(ctx->out_file);

    free(ctx);
}