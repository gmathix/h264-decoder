//
// Created by gmathix on 3/28/26.
//

#ifndef TOY_H264_SLICEUTIL_H
#define TOY_H264_SLICEUTIL_H




#define SLICE_P            0
#define SLICE_B            1
#define SLICE_I            2
#define SLICE_SP           3
#define SLICE_SI           4
#define SLICE_P_BIS        5
#define SLICE_B_BIS        6
#define SLICE_I_BIS        7
#define SLICE_SP_BIS       8
#define SLICE_SI_BIS       9

#define IS_P_SLICE(a)  (((a) == SLICE_P) || ((a) == SLICE_P_BIS))
#define IS_B_SLICE(a)  (((a) == SLICE_B) || ((a) == SLICE_B_BIS))
#define IS_I_SLICE(a)  (((a) == SLICE_I) || ((a) == SLICE_I_BIS))
#define IS_SP_SLICE(a) (((a) == SLICE_SP) || ((a) == SLICE_SP_BIS))
#define IS_SI_SLICE(a) (((a) == SLICE_SI) || ((a) == SLICE_SI_BIS))



static char* slice_type_to_string(uint32_t slice_type) {
    if (IS_I_SLICE(slice_type))  return "I";
    if (IS_P_SLICE(slice_type))  return "P";
    if (IS_B_SLICE(slice_type))  return "B";
    if (IS_SI_SLICE(slice_type)) return "SI";
    if (IS_SP_SLICE(slice_type)) return "SP";
    return "UNDEF";
}




#endif //TOY_H264_SLICEUTIL_H