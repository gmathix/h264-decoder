//
// Created by gmathix on 3/20/26.
//

#ifndef TOY_H264_FORMULAS_H
#define TOY_H264_FORMULAS_H

#include <stdint.h>
#include <math.h>
#include <stdio.h>

#include "../decoder.h"



static inline double  _abs(double x)           { return x >= 0 ? x : -x; }
static inline int32_t _min(int32_t x, int32_t y) { return x <= y ? x : y; }
static inline double  _max(double x, double y) { return x >= y ? x : y; }
static inline double  _log2(double x)          { return log2(x); }
static inline double  _log10(double x)         { return log10(x); }
static inline double  _sqrt(double x)          { return sqrt(x); }

static inline int32_t _floor(double x)         { return (int32_t) x; }
static inline int32_t _ceil(double x)          { return (int32_t)x + (x > (int32_t)x ? 1 : 0); }
static inline int32_t _sign(double x)          { return x >= 0 ? 1 : -1; }
static inline int32_t _round(double x)         { return _sign(x) * _floor(_abs(x) + 0.5); }




static void binprintf(int v, int length)
{
    for (int i = 31; i >= 32-length; i--) {
        putchar((v & (1 << i)) ? '1' : '0');
    }
}


static inline int32_t _clip3(int32_t x, int32_t y, int32_t z) {
    if (z < x) return x;
    if (z > y) return y;
    return z;
}

static inline int32_t _clip1y(int32_t x, CodecContext *ctx) {
    return _clip3(0, (1 << ctx->ps->sps->bit_depth_luma) - 1, x);
}

static inline int32_t _clip1c(int32_t x, CodecContext *ctx) {
    return _clip3(0, (1 << ctx->ps->sps->bit_depth_chroma) - 1, x);
}

static inline int32_t _inverse_raster_scan(int32_t a, int32_t b, int32_t c, int32_t d, int32_t e) {
    return e == 0
        ? (a % (d / b)) * b
        : (a / (d / b)) * c;
}

static inline double _median(double x, double y, double z) {
    return x + y + z - _min(x, _min(y, z)) - _max(x, _max(y, z));
}


static inline void nothing() { void *nothing = NULL; }




#endif //TOY_H264_FORMULAS_H