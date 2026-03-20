//
// Created by gmathix on 3/20/26.
//

#include <stdlib.h>

#include "decoder.h"

#include <stdio.h>

#include "annexb.h"
#include "nal.h"

CodecContext *decoder_init(const uint8_t *data, size_t size) {
    if (data == NULL) return NULL;

    CodecContext *context = calloc(1, sizeof(CodecContext));
    if (!context) {
        free(context);
        return NULL;
    };

    context->data = data;
    context->size = size;

    BitReader br = make_br(context->data, context->size);
    context->br = &br;

    ParamSets *ps = calloc(1, sizeof(ParamSets));
    context->ps = ps;


    context->initialized = true;

    return context;
}


void decoder_run(CodecContext *context) {
    if (!context->initialized) return;

    int num_nals = count_nals(context->data, context->size);
    NalUnit *nal_units = malloc(num_nals * sizeof(NalUnit));
    fill_nal_units(context->data, context->size, nal_units, num_nals);

    int i = 0;
    while (i < num_nals) {
        dispatch_nal_unit(&nal_units[i], context->ps);

        i++;
    }
}