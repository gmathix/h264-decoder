//
// Created by gmathix on 8/25/26.
//

#ifndef UNDO264_TRANSFORM_COMMON_H
#define UNDO264_TRANSFORM_COMMON_H

#include "stdint.h"

static const uint8_t field_scan_4x4[4][4] = {
    { 0,  2,  8, 12},
    { 1,  5,  9, 13},
    { 3,  6, 10, 14},
    { 4,  7, 11, 15}
};

static const uint8_t blk_scan_8x8[8][8] = {
    { 0,  1,  5,  6, 14, 15, 27, 28},
    { 2,  4,  7, 13, 16, 26, 29, 42},
    { 3,  8, 12, 17, 25, 30, 41, 43},
    { 9, 11, 18, 24, 31, 40, 44, 53},
    {10, 19, 23, 32, 39, 45, 52, 54},
    {20, 22, 33, 38, 46, 51, 55, 60},
    {21, 34, 37, 47, 50, 56, 59, 61},
    {35, 36, 48, 49, 57, 58, 62, 63}
};

static const uint8_t field_scan_8x8[8][8] = {
    { 0,  3,  8, 15, 22, 30, 38, 52},
    { 1,  4, 14, 21, 29, 37, 45, 53},
    { 2,  7, 16, 23, 31, 39, 46, 58},
    { 5,  9, 20, 28, 36, 44, 51, 59},
    { 6, 13, 24, 32, 40, 47, 54, 60},
    {10, 17, 25, 33, 41, 48, 55, 61},
    {11, 18, 26, 34, 42, 49, 56, 62},
    {12, 19, 27, 35, 43, 50, 57, 63}
};

static const int16_t hadamard_4x4_mat[4][4] = {
    { 1,  1,  1,  1},
    { 1,  1, -1, -1},
    { 1, -1, -1,  1},
    { 1, -1,  1, -1}
};

static const int16_t hadamard_2x2_mat[2][2] = {
    { 1,  1},
    { 1, -1}
};


static always_inline void scaling_residual_4x4_lshift(int shift, int16_t (*scale)[4], int16_t c[4][4], int16_t d[4][4], bool is_luma, Undo264Context *ctx) {
      for (int i = 0; i < 4; i++) {
            for (int j =0 ; j < 4; j++) {
                  d[i][j] = lshift(c[i][j] * scale[i][j], shift);
            }
      }
}
static always_inline void scaling_residual_4x4_rshift_min(int shift, int16_t (*scale)[4], int16_t c[4][4], int16_t d[4][4], bool is_luma, Undo264Context *ctx) {
      for (int i = 0; i < 4; i++) {
            for (int j = 0 ; j < 4; j++) {
                  d[i][j] = rshift_min(c[i][j] * scale[i][j], shift);
            }
      }
}

static always_inline void scaling_residual_8x8_lshift(int shift, int16_t (*scale)[8], int16_t c[8][8], int16_t d[8][8], bool is_luma, Undo264Context *ctx) {
      for (int i = 0; i < 8; i++) {
            for (int j =0 ; j < 8; j++) {
                  d[i][j] = lshift(c[i][j] * scale[i][j], shift);
            }
      }
}
static always_inline void scaling_residual_8x8_rshift_min(int shift, int16_t (*scale)[8], int16_t c[8][8], int16_t d[8][8], bool is_luma, Undo264Context *ctx) {
      for (int i = 0; i < 8; i++) {
            for (int j = 0 ; j < 8; j++) {
                  d[i][j] = rshift_min(c[i][j] * scale[i][j], shift);
            }
      }
}


static always_inline void inverse_4x4_coeff_scaling_scan(int16_t vals[16], int16_t out[4][4]) {
      out[0][0] = vals[0];   out[0][1] = vals[1];   out[0][2] = vals[5];   out[0][3] = vals[6];
      out[1][0] = vals[2];   out[1][1] = vals[4];   out[1][2] = vals[7];   out[1][3] = vals[12];
      out[2][0] = vals[3];   out[2][1] = vals[8];   out[2][2] = vals[11];  out[2][3] = vals[13];
      out[3][0] = vals[9];   out[3][1] = vals[10];  out[3][2] = vals[14];  out[3][3] = vals[15];
}
static always_inline void inverse_4x4_coeff_scaling_scan_dc(int16_t AC[15], int dc, int16_t out[4][4]) {
      out[0][0] = dc;      out[0][1] = AC[0];   out[0][2] = AC[4];   out[0][3] = AC[5];
      out[1][0] = AC[1];   out[1][1] = AC[3];   out[1][2] = AC[6];   out[1][3] = AC[11];
      out[2][0] = AC[2];   out[2][1] = AC[7];   out[2][2] = AC[10];  out[2][3] = AC[12];
      out[3][0] = AC[8];   out[3][1] = AC[9];   out[3][2] = AC[13];  out[3][3] = AC[14];
}


static always_inline void inverse_8x8_coeff_scaling_scan(int16_t vals[64], int16_t out[8][8]) {
      out[ 0][ 0]=vals[ 0];  out[ 0][ 1]=vals[ 1];  out[ 0][ 2]=vals[ 5];  out[ 0][ 3]=vals[ 6];
      out[ 0][ 4]=vals[14];  out[ 0][ 5]=vals[15];  out[ 0][ 6]=vals[27];  out[ 0][ 7]=vals[28];

      out[ 1][ 0]=vals[ 2];  out[ 1][ 1]=vals[ 4];  out[ 1][ 2]=vals[ 7];  out[ 1][ 3]=vals[13];
      out[ 1][ 4]=vals[16];  out[ 1][ 5]=vals[26];  out[ 1][ 6]=vals[29];  out[ 1][ 7]=vals[42];

      out[ 2][ 0]=vals[ 3];  out[ 2][ 1]=vals[ 8];  out[ 2][ 2]=vals[12];  out[ 2][ 3]=vals[17];
      out[ 2][ 4]=vals[25];  out[ 2][ 5]=vals[30];  out[ 2][ 6]=vals[41];  out[ 2][ 7]=vals[43];

      out[ 3][ 0]=vals[ 9];  out[ 3][ 1]=vals[11];  out[ 3][ 2]=vals[18];  out[ 3][ 3]=vals[24];
      out[ 3][ 4]=vals[31];  out[ 3][ 5]=vals[40];  out[ 3][ 6]=vals[44];  out[ 3][ 7]=vals[53];

      out[ 4][ 0]=vals[10];  out[ 4][ 1]=vals[19];  out[ 4][ 2]=vals[23];  out[ 4][ 3]=vals[32];
      out[ 4][ 4]=vals[39];  out[ 4][ 5]=vals[45];  out[ 4][ 6]=vals[52];  out[ 4][ 7]=vals[54];

      out[ 5][ 0]=vals[20];  out[ 5][ 1]=vals[22];  out[ 5][ 2]=vals[33];  out[ 5][ 3]=vals[38];
      out[ 5][ 4]=vals[46];  out[ 5][ 5]=vals[51];  out[ 5][ 6]=vals[55];  out[ 5][ 7]=vals[60];

      out[ 6][ 0]=vals[21];  out[ 6][ 1]=vals[34];  out[ 6][ 2]=vals[37];  out[ 6][ 3]=vals[47];
      out[ 6][ 4]=vals[50];  out[ 6][ 5]=vals[56];  out[ 6][ 6]=vals[59];  out[ 6][ 7]=vals[61];

      out[ 7][ 0]=vals[35];  out[ 7][ 1]=vals[36];  out[ 7][ 2]=vals[48];  out[ 7][ 3]=vals[49];
      out[ 7][ 4]=vals[57];  out[ 7][ 5]=vals[58];  out[ 7][ 6]=vals[62];  out[ 7][ 7]=vals[63];
}

#endif //UNDO264_TRANSFORM_COMMON_H