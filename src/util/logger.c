//
// Created by gmathix on 4/5/26.
//

#include "logger.h"

#include <stdlib.h>
#include <string.h>

#include "formulas.h"
#include "mbutil.h"
#include "sliceutil.h"


void print_annexb_header(size_t size, int nal_ref_idc, int nal_unit_type) {
    printf("Annex B NALU, len %lu, nal_ref_idc %d, nal_unit_type %d\n\n",
        size, nal_ref_idc, nal_unit_type);
}

void print_annexb_line_info(size_t g_offset, char *title, char *param_name, BitReader *br) {
    printf("@%lu", g_offset + br->byte_pos*8 + br->bit_pos);
    spaces(MAX_BIT_OFFSET_SIZE - nb_digits(g_offset + br->byte_pos*8 + br->bit_pos));

    spaces(MAX_TITLE_OFFSET_SIZE - strlen(title));
    printf("%s", title);

    printf(": %s", param_name);
    spaces(MAX_PARAM_NAME_SIZE - strlen(param_name));
}

void print_annexb_line_value(int32_t value) {
    printf("(");
    spaces(MAX_PARAM_VALUE_INT16_SIZE - nb_digits(value));
    printf("%d)\n", value);
}




void print_macroblock_header(int poc, int mbAddr, int slice_num, int slice_type) {
    printf("\n********* POC: %d MB: %d Slice: %d Type %d (%s) *********\n",
        poc, mbAddr,
        slice_num, slice_type, slice_type_to_string(slice_type));
}

void print_slice_line_info(size_t g_offset, char *name, BitReader *br) {
    printf("@%lu", g_offset + br->byte_pos*8 + br->bit_pos);
    spaces(MAX_BIT_OFFSET_SIZE - nb_digits(g_offset + br->byte_pos*8 + br->bit_pos));

    printf("%s", name);
    spaces(MAX_INFO_SIZE - strlen(name));
}

void print_slice_line_value(int32_t value) {
    printf("(");
    spaces(MAX_PARAM_VALUE_INT16_SIZE - nb_digits(value));
    printf("%d)\n", value);
}


void print_bit_pos(size_t g_offset, BitReader *br) {
    printf("@%lu", g_offset + br->byte_pos*8 + br->bit_pos);
    spaces(MAX_BIT_OFFSET_SIZE - nb_digits(g_offset + br->byte_pos*8 + br->bit_pos));
}

void print_info_only(char *info) {
    printf("%s", info);
    spaces(MAX_INFO_SIZE - strlen(info));
}

void print_bits(int32_t bits, int length) {
    spaces(MAX_PARAM_VALUE_BIN_SIZE - length);
    binprintf(bits, length);
}

void print_value(int32_t value) {
    printf("(");
    spaces(MAX_PARAM_VALUE_INT16_SIZE - nb_digits(value));
    printf("%d)\n", value);
}