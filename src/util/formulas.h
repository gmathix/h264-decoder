//
// Created by gmathix on 3/20/26.
//

#ifndef TOY_H264_FORMULAS_H
#define TOY_H264_FORMULAS_H

#include <stdint.h>
#include <math.h>
#include <stdio.h>


#define ABS(x) (x >= 0 ? (x) : -(x))
#define CLIP3(min, max, x) x > max ? max : x < min ? min : x

static always_inline int32_t  _abs(int32_t x)           { return x >= 0 ? x : -x; }
static always_inline int32_t _min(int32_t x, int32_t y) { return x <= y ? x : y; }
static always_inline int32_t _max(int x, int y) { return x >= y ? x : y; }
static always_inline double  _log2(double x)          { return log2(x); }
static always_inline double  _log10(double x)         { return log10(x); }
static always_inline double  _sqrt(double x)          { return sqrt(x); }

static always_inline int32_t _floor(double x)         { return (int32_t) (x < 0 ? x - 1.0 : x); }
static always_inline int32_t _ceil(double x)          { return (int32_t)x + (x > (int32_t)x ? 1 : 0); }
static always_inline int32_t _sign(double x)          { return x >= 0 ? 1 : -1; }
static always_inline int32_t _round(double x)         { return _sign(x) * _floor(_abs(x) + 0.5); }

static always_inline int32_t _minPositive(int x, int y) { return (x >= 0 && y >= 0) ? _min(x, y) : _max(x, y); }



static always_inline int32_t rshift_min(int32_t n, int16_t qp) {
    return (n + (1 << (-qp-1))) >> (-qp);
}

static always_inline int32_t rshift_norm(int32_t n, int32_t f, int32_t s) {
    return (n << f) >> s;
}

static always_inline int32_t lshift(int32_t n, int16_t qp) {
    return n * (1 << qp);
}


static void binprintf(int v, int length)
{
    for (int i = 31; i >= 32-length; i--) {
        putchar((v & (1 << i)) ? '1' : '0');
    }
}


static always_inline int32_t _clip3(int32_t min, int32_t max, int32_t n) {
    return n > max ? max : (n < min ? min : n);
}

static always_inline int32_t _clip2(int32_t x, int32_t y) {
    return y > x ? y : x;
}

static always_inline uint32_t _clip1y(int x, int max) {
    if ((unsigned)x > (unsigned)max) x = x < 0 ? 0 : max;
    return x;
}

static always_inline uint32_t _clip1c(int x, int max) {
    if ((unsigned)x > (unsigned)max) x = x < 0 ? 0 : max;
    return x;
}

static always_inline int32_t _inverse_raster_scan(int32_t a, int32_t b, int32_t c, int32_t d, int32_t e) {
    return e == 0
        ? (a % (d / b)) * b
        : (a / (d / b)) * c;
}

static always_inline int _median(int x, int y, int z) {
    return x + y + z - _min(x, _min(y, z)) - _max(x, _max(y, z));
}




#endif //TOY_H264_FORMULAS_H