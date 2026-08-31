//
// Created by gmathix on 3/20/26.
//



#include "decoder.h"

#include "annexb.h"
#include "dequant.h"
#include "dpb.h"
#include "mb.h"
#include "ps.h"
#include "dsp_init.h"
#include "tests/profiler.h"
#include "util/bitreader.h"


#include "cavlc.h"
#include "cabac.h"
#include "deblock.h"
#include "intra.h"
#include "picture.h"

#include "tests/profiler.h"
#include "util/expgolomb.h"
#include "util/mbutil.h"
#include "util/predutil.h"
#include "util/sliceutil.h"



#define CABAC 1
#include "slice.c"
#undef CABAC
#define CABAC 0
#include "slice.c"


int debugging = 0;
int frame_debug = -1;
int frame_num_debug = -1;
int poc_debug = 60;
int mb_debug = 395;
int nb_frames_before_stop = -1;


Undo264Context *decoder_init(const uint8_t *data, size_t size, char *out_path, char *log_path, bool dump_monochrome) {

    if (data == NULL) return NULL;

    Undo264Context *ctx = calloc(1, sizeof(Undo264Context));
    if (!ctx) {
        return NULL;
    }

    ctx->data = data;
    ctx->size = size;

    CabacContext *cactx = make_cactx();
    ctx->cactx = cactx;

    BitReader *br = malloc(sizeof(BitReader));
    ctx->br = br;
    ctx->global_bit_offset = 0;

    ParamSets *ps = calloc(1, sizeof(ParamSets));
    ctx->ps = ps;

    DSPContext *dsp_context = calloc(1, sizeof(DSPContext));
    ctx->dsp = dsp_context;
    dsp_init(ctx->dsp);


    ctx->current_slice = slice_alloc();
    ctx->dpb = make_dbp(ctx);
    ctx->pool = calloc(1, sizeof(PicturePool));
    ctx->pic_pool_initialized = false;


    ctx->currMb = calloc(1, sizeof(Macroblock));
    ctx->prevQPY = 0;

    ctx->levelScale4x4 = calloc(6, sizeof( int16_t[52][4][4] ));
    ctx->levelScale8x8 = calloc(2, sizeof( int16_t[52][8][8] ));

    ctx->out_path = out_path;
    ctx->out_file = fopen(ctx->out_path, "wb");
    ctx->dump_monochrome = dump_monochrome;
    if (!ctx->out_file) {
        perror("fopen");
        exit(121);
    }
    setvbuf(ctx->out_file, NULL, _IOFBF, (size_t) 8 * 1920*1080*1.5); // 8 frame buffer for HD

    ctx->log_path = log_path;
    ctx->log_file = fopen(ctx->log_path, "w");



    ctx->mc_scratch_buffers[(16 * 16) / 4 - 1] = ctx->buf_16x16;
    ctx->mc_scratch_buffers[(16 *  8) / 4 - 1] = ctx->buf_16x8;
    ctx->mc_scratch_buffers[( 8 *  8) / 4 - 1] = ctx->buf_8x8;
    ctx->mc_scratch_buffers[( 8 *  4) / 4 - 1] = ctx->buf_8x4;
    ctx->mc_scratch_buffers[( 4 *  4) / 4 - 1] = ctx->buf_4x4;
    ctx->mc_scratch_buffers[( 4 *  2) / 4 - 1] = ctx->buf_4x2;
    ctx->mc_scratch_buffers[ (2 *  2) / 4 - 1] = ctx->buf_2x2;

    ctx->mc_temp_bi_buffers[(16 * 16) / 4 - 1] = ctx->temp_bi_buf_16x16;
    ctx->mc_temp_bi_buffers[(16 *  8) / 4 - 1] = ctx->temp_bi_buf_16x8;
    ctx->mc_temp_bi_buffers[( 8 *  8) / 4 - 1] = ctx->temp_bi_buf_8x8;
    ctx->mc_temp_bi_buffers[( 8 *  4) / 4 - 1] = ctx->temp_bi_buf_8x4;
    ctx->mc_temp_bi_buffers[( 4 *  4) / 4 - 1] = ctx->temp_bi_buf_4x4;
    ctx->mc_temp_bi_buffers[( 4 *  2) / 4 - 1] = ctx->temp_bi_buf_4x2;
    ctx->mc_temp_bi_buffers[( 2 *  2) / 4 - 1] = ctx->temp_bi_buf_2x2;

    ctx->qpel_pass_buffers[16 / 4 - 1] = ctx->qpel_pass_buf_16;
    ctx->qpel_pass_buffers[ 8 / 4 - 1] = ctx->qpel_pass_buf_8;
    ctx->qpel_pass_buffers[ 4 / 4 - 1] = ctx->qpel_pass_buf_4;


    ctx->prf = malloc(sizeof(Profiler));
    profiler_init(ctx->prf);


    ctx->initialized = true;

    return ctx;
}




// yes this now does not to belong to nal.c anymore because a NALcoholic deleted that file
int dispatch_nal_unit(NalUnit *nal_unit, Undo264Context *ctx) {

    bitreader_init(ctx->br, nal_unit->data, nal_unit->size);

    switch (nal_unit->type) {
        case NAL_SEI: break;
        case NAL_SPS:
            decode_sps(ctx); ctx->global_bit_offset += bitreader_bits_consumed(ctx->br);
            int num_mbs = ctx->ps->sps->pic_height_in_map_units * ctx->ps->sps->pic_width_in_mbs;
            if (!ctx->pic_pool_initialized || num_mbs != ctx->num_mbs) {
                pic_pool_init(ctx->pool, ctx);
                ctx->pic_pool_initialized = true;
            }
            break;
        case NAL_PPS: decode_pps(ctx); ctx->global_bit_offset += bitreader_bits_consumed(ctx->br); break;


        case NAL_CODED_SLICE_OF_NON_IDR_PICTURE:
        case NAL_CODED_SLICE_OF_IDR_PICTURE:
        case NAL_CODED_SLICE_DATA_PARTITION_A:
        case NAL_CODED_SLICE_DATA_PARTITION_B:
        case NAL_CODED_SLICE_DATA_PARTITION_C:
        case NAL_CODED_SLICE_OF_AUX_CODED_PICTURE:
        case NAL_CODED_SLICE_EXTENSION: {

            profiler_start_frame(ctx->prf);

            SliceHeader *sh = read_slice_header(nal_unit, ctx);

            if (sh->pps->cabac_flag) {
                decode_slice_cabac(sh, nal_unit, ctx);
            } else {
                decode_slice_cavlc(sh, nal_unit, ctx);
            }

            Slice *slice = ctx->current_slice;
            deblock_slice(ctx->curr_pic, sh, ctx);

            #ifdef SLICES_LOG
                printf("done slice %lu %s(frame_num %d, pic %lu)\n\n",
                    ctx->prf->total_frames, slice->p_pic->sh->idr_pic_flag ? "(IDR) " : "", sh->frame_num, ctx->prf->total_frames);
            #endif
            if (ctx->prf->total_frames % 100 == 0 && ctx->prf->total_frames > 0) {
                printf("Progress: finished picture #%lu\n", ctx->prf->total_frames);
            }

            if (slice->num_mbs + sh->first_mb == slice->p_pic->num_mbs ||
                slice->num_mbs + sh->first_mb == slice->p_pic->num_mbs+1) { // end of picture

                store_picture(ctx->dpb, ctx->curr_pic);
                profiler_end_frame(ctx->prf);
            }





            ctx->global_bit_offset += bitreader_bits_consumed(ctx->br);
            break;
        }

        case NAL_UNSPECIFIED:
        case NAL_RS16:
        case NAL_RS17:
        case NAL_RS18:
        case NAL_RS21:
        case NAL_RS22:
        case NAL_RS23:
        case NAL_UNSPEC24:
        case NAL_UNSPEC25:
        case NAL_UNSPEC26:
        case NAL_UNSPEC27:
        case NAL_UNSPEC28:
        case NAL_UNSPEC29:
        case NAL_UNSPEC30:
        case NAL_UNSPEC31:        return 0;

        default: return -1;
    }
}


void decoder_run(Undo264Context *ctx) {
    if (!ctx->initialized) return;

    BitReader nal_br = make_br(ctx->data, ctx->size);

    https://www.youtube.com/watch?v=RrESvSRNpeo
    {
        NalUnit *nal = next_nal_unit(&nal_br);

        if (nal) {
            dispatch_nal_unit(nal, ctx);

            free(nal->data);
            free(nal);

            if (bitreader_bits_remaining(&nal_br) > 8 && ctx->prf->total_frames <= nb_frames_before_stop) {
                goto https;
            }
        }
    }

	dpb_flush(ctx->dpb);
	fflush(ctx->out_file);
}

void decoder_free_metadata(Undo264Context *ctx) {
    free(ctx->mb_metadata);
    free(ctx->total_coeffs);
}

/* caller's job to make sure metadata gets free beforehand */
void decoder_alloc_metadata(Undo264Context *ctx) {

    ctx->num_mbs = (int32_t)ctx->ps->sps->pic_width_in_mbs * (int32_t)ctx->ps->sps->pic_height_in_map_units;

    ctx->mb_metadata = calloc(ctx->num_mbs, sizeof( MacroblockMetadata));
    ctx->total_coeffs = calloc(ctx->num_mbs, sizeof( uint8_t [24]));


    ctx->mb_metadata_initialized = true;
}

void decoder_free(Undo264Context *ctx) {
    decoder_free_metadata(ctx);
    pic_pool_free(ctx->pool, ctx);
    free_cactx(ctx->cactx);

    munmap((void*)ctx->data, ctx->size);
    free(ctx->br);
    free(ctx->prf);
    free(ctx->dsp);

    free(ctx->ps->sps);
    free(ctx->ps->pps);
    free(ctx->ps);

    free(ctx->levelScale4x4);
    free(ctx->levelScale8x8);

    free(ctx->current_slice);


    free(ctx->currMb);
    // free(ctx->prevMb);


    dpb_free(ctx->dpb);

    fclose(ctx->out_file);

    free(ctx);
}


char* profile_to_string(int profile) {
    switch (profile) {
        case PROFILE_CAVLC_444: return "CAVLC_444";
        case PROFILE_BASELINE: return "Baseline";
        case PROFILE_MAIN: return "Main";
        case PROFILE_EXTENDED: return "Extended";
        case PROFILE_HIGH: return "High";
        case PROFILE_HIGH_10: return "High 10-bit";
        case PROFILE_HIGH_422: return "High 4:2:2";
        case PROFILE_HIGH_PRED_444: return "High 4:4:4";
        default: return "Unknown";
    }
}

char* blockType_to_string(int bt) {
    switch (bt) {
        case LUMA_INTRA_16x16_DC_LEVEL: return "Lum16DC";
        case LUMA_INTRA_16x16_AC_LEVEL: return "Lum16AC";
        case CB_INTRA_16x16_DC_LEVEL: return "Cb16DC";
        case CB_INTRA_16x16_AC_LEVEL: return "Cb16AC";
        case CR_INTRA_16x16_DC_LEVEL: return "Cr16DC";
        case CR_INTRA_16x16_AC_LEVEL: return "Cr16AC";
        case LUMA_LEVEL_4x4: return "Lum4";
        case CHROMA_DC_LEVEL: return "ChrDC";
        case CHROMA_AC_LEVEL: return "ChrAC";
        case CB_LEVEL_4x4: return "Cb4";
        case CR_LEVEL_4x4: return "Cr4";
        case LUMA_LEVEL_8x8: return "Luma8";
        default: return "Block";
    }
}