//
// Created by gmathix on 4/6/26.
//

#ifndef TOY_H264_GLOBAL_H
#define TOY_H264_GLOBAL_H



#define ALL_LOG     0

#define CAVLC_LOG   (ALL_LOG    |   0)
#define NAL_LOG     (ALL_LOG    |   0)


#define ALWAYS_INLINE inline __attribute__((always_inline))
#define OPTIMIZE_O0   __attribute__((optimize("O0")))
#define OPTIMIZE_O1   __attribute__((optimize("O1")))
#define OPTIMIZE_O2   __attribute__((optimize("O2")))
#define OPTIMIZE_O3   __attribute__((optimize("O3")))



#endif //TOY_H264_GLOBAL_H