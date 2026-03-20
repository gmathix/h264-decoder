//
// Created by gmathix on 3/12/26.
//


#include "nal.h"

#include <stdio.h>


int dispatch_nal_unit(NalUnit *nal_unit, ParamSets *ps) {

    BitReader br = make_br(nal_unit->data, nal_unit->size);

    switch (nal_unit->type) {
        case NAL_SPS: printf("dispatching SPS unit\n"); return decode_sps(&br, ps);
        case NAL_PPS: printf("dispatching PPS unit\n"); return decode_pps(&br, ps);


        case NAL_CODED_SLICE_OF_NON_IDR_PICTURE:
        case NAL_CODED_SLICE_OF_IDR_PICTURE:
        case NAL_CODED_SLICE_DATA_PARTITION_A:
        case NAL_CODED_SLICE_DATA_PARTITION_B:
        case NAL_CODED_SLICE_DATA_PARTITION_C:
        case NAL_CODED_SLICE_OF_AUX_CODED_PICTURE:
        case NAL_CODED_SLICE_EXTENSION:
            printf("dispatching slice\n");
            return -1; /// TODO: dispatch these to slice.c



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
