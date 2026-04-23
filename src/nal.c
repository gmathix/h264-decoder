//
// Created by gmathix on 3/12/26.
//


#include "global.h"
#include "nal.h"
#include "slice.h"

#include <stdio.h>


int dispatch_nal_unit(NalUnit *nal_unit, CodecContext *ctx) {

    bitreader_init(ctx->br, nal_unit->data, nal_unit->size);

#if NAL_LOG
    printf("\nAnnex B NALU, len %lu, nal_ref_idc %d, nal_unit_type %d\n",
        nal_unit->size+1, nal_unit->ref_idc, nal_unit->type);
#endif


    switch (nal_unit->type) {
        case NAL_SEI: break;
        case NAL_SPS: decode_sps(ctx->global_bit_offset, ctx->br, ctx->ps); ctx->global_bit_offset += bitreader_bits_consumed(ctx->br); break;
        case NAL_PPS: decode_pps(ctx->global_bit_offset, ctx->br, ctx->ps); ctx->global_bit_offset += bitreader_bits_consumed(ctx->br); break;


        case NAL_CODED_SLICE_OF_NON_IDR_PICTURE:
        case NAL_CODED_SLICE_OF_IDR_PICTURE:
        case NAL_CODED_SLICE_DATA_PARTITION_A:
        case NAL_CODED_SLICE_DATA_PARTITION_B:
        case NAL_CODED_SLICE_DATA_PARTITION_C:
        case NAL_CODED_SLICE_OF_AUX_CODED_PICTURE:
        case NAL_CODED_SLICE_EXTENSION:
            decode_slice(nal_unit, ctx);
            ctx->global_bit_offset += bitreader_bits_consumed(ctx->br);
            break;


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