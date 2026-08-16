//
// Created by gmathix on 3/20/26.
//

#ifndef TOY_H264_FORMULAS_H
#define TOY_H264_FORMULAS_H

#include <stdint.h>
#include <math.h>
#include <stdio.h>



static ALWAYS_INLINE int32_t  _abs(int32_t x)           { return x >= 0 ? x : -x; }
static ALWAYS_INLINE int32_t _min(int32_t x, int32_t y) { return x <= y ? x : y; }
static ALWAYS_INLINE int32_t _max(int x, int y) { return x >= y ? x : y; }
static ALWAYS_INLINE double  _log2(double x)          { return log2(x); }
static ALWAYS_INLINE double  _log10(double x)         { return log10(x); }
static ALWAYS_INLINE double  _sqrt(double x)          { return sqrt(x); }

static ALWAYS_INLINE int32_t _floor(double x)         { return (int32_t) (x < 0 ? x - 1.0 : x); }
static ALWAYS_INLINE int32_t _ceil(double x)          { return (int32_t)x + (x > (int32_t)x ? 1 : 0); }
static ALWAYS_INLINE int32_t _sign(double x)          { return x >= 0 ? 1 : -1; }
static ALWAYS_INLINE int32_t _round(double x)         { return _sign(x) * _floor(_abs(x) + 0.5); }

static ALWAYS_INLINE int32_t _minPositive(int x, int y) { return (x >= 0 && y >= 0) ? _min(x, y) : _max(x, y); }



static ALWAYS_INLINE int32_t rshift_min(int32_t n, int16_t qp) {
    return (n + (1 << (-qp-1))) >> (-qp);
}

static ALWAYS_INLINE int32_t rshift_norm(int32_t n, int32_t f, int32_t s) {
    return (n << f) >> s;
}

static ALWAYS_INLINE int32_t lshift(int32_t n, int16_t qp) {
    return n * (1 << qp);
}


static void binprintf(int v, int length)
{
    for (int i = 31; i >= 32-length; i--) {
        putchar((v & (1 << i)) ? '1' : '0');
    }
}


static ALWAYS_INLINE int32_t _clip3(int32_t x, int32_t y, int32_t z) {
    if (z < x) return x;
    if (z > y) return y;
    return z;
}

static ALWAYS_INLINE int32_t _clip2(int32_t x, int32_t y) {
    return y > x ? y : x;
}

static ALWAYS_INLINE uint32_t _clip1y(int32_t x, int bit_depth) {
    return (uint32_t) _clip3(0, (1 << bit_depth) - 1, x);
}

static ALWAYS_INLINE uint32_t _clip1c(int32_t x, int bit_depth) {
    return (uint32_t) _clip3(0, (1 << bit_depth) - 1, x);
}

static ALWAYS_INLINE int32_t _inverse_raster_scan(int32_t a, int32_t b, int32_t c, int32_t d, int32_t e) {
    return e == 0
        ? (a % (d / b)) * b
        : (a / (d / b)) * c;
}

static ALWAYS_INLINE int _median(int x, int y, int z) {
    return x + y + z - _min(x, _min(y, z)) - _max(x, _max(y, z));
}


static ALWAYS_INLINE void nothing() { void *nothing = NULL; }




#endif //TOY_H264_FORMULAS_H