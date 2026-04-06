//
// Created by gmathix on 4/5/26.
//

#ifndef TOY_H264_LOGGER_H
#define TOY_H264_LOGGER_H


#include "bitreader.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>


#define MAX_BIT_OFFSET_SIZE         15
#define MAX_TITLE_OFFSET_SIZE        4
#define MAX_PARAM_NAME_SIZE         47
#define MAX_INFO_SIZE              (MAX_TITLE_OFFSET_SIZE+MAX_PARAM_NAME_SIZE)
#define MAX_PARAM_VALUE_BIN_SIZE    16
#define MAX_PARAM_VALUE_INT16_SIZE   5



typedef struct SliceLine {
    size_t bit_pos;
    uint16_t mb_type;
    uint16_t intra_chroma_pred_mode;
    uint16_t mb_qp_delta;
    char *title;
    int64_t value;
} SliceLine ;


typedef struct {
    size_t size;
    int nal_ref_idc;
    int nal_unit_type;
    size_t nb_lines;
    SliceLine *slice_lines;
} SliceSequence ;



static SliceSequence make_slice_seq(size_t size, int nal_ref_idc, int nal_unit_type) {
    SliceSequence seq = {0};
    seq.size = size;
    seq.nal_ref_idc = nal_ref_idc;
    seq.nal_unit_type = nal_unit_type;
    return seq;
}

static size_t nb_digits(double nb) {
    if (nb < 0) return nb_digits(nb*-1)+1;
    if (nb < 10) return 1;
    return nb_digits(nb/10) + 1;
}

static void spaces(int n) {
    for (int i = 0; i < n; i++) printf(" ");
}


void print_annexb_header(size_t size, int nal_ref_idc, int nal_unit_type);

void print_annexb_line_info(size_t g_offset, char *title, char *param_name, BitReader *br);
void print_annexb_line_value(int32_t value);

void print_macroblock_header(int poc, int mbAddr, int slice_num, int slice_type);
void print_slice_line_info(size_t g_offset, char *name, BitReader *br);
void print_slice_line_value(int32_t value);

void print_bit_pos(size_t g_offset, BitReader *br);
void print_info_only(char *info);
void print_bits(int32_t bits, int length);
void print_value(int32_t value);




#endif //TOY_H264_LOGGER_H