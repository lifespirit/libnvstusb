/*
 * Timer 2 frame-period calculations for the NVIDIA USB shutter emitter.
 *
 * The firmware interpretation of the T2 reload register is partly
 * reverse-engineered. These functions calculate register values only;
 * they cannot prove the IR/light-shutter waveform or phase locking.
 *
 * Keep BOTH paths: the legacy calculation truncates a frame period to
 * whole microseconds before converting to nominal 12 MHz ticks. The
 * experimental exact calculation rounds the period directly in ticks.
 */
#ifndef NVSTUSB_TIMING_H
#define NVSTUSB_TIMING_H

#include <math.h>
#include <stdint.h>

#define NVSTUSB_TIMER2_HZ INT64_C(12000000)

/* Bit-for-bit equivalent to (int32_t) NVSTUSB_T2_COUNT(frameTime) where
 * frameTime is int32_t(1000000.0 / rate). */
static inline int32_t nvstusb_frame_reload_legacy(float rate)
{
    int32_t frame_us = (int32_t)(1000000.0 / rate);
    return (int32_t)(1 - (int64_t)frame_us * (NVSTUSB_TIMER2_HZ / 1000000));
}

/* Precise *nominal* period: round once, at the underlying timer tick.
 * Both formulas require the same valid positive rate supplied by
 * nvstusb_set_rate(). */
static inline int32_t nvstusb_frame_reload_exact(float rate)
{
    int64_t ticks = (int64_t)llround((double)NVSTUSB_TIMER2_HZ / rate);
    return (int32_t)(1 - ticks);
}

static inline double nvstusb_frame_reload_period_us(int32_t reload)
{
    return (double)(INT64_C(1) - reload) * 1000000.0 /
           (double)NVSTUSB_TIMER2_HZ;
}

#endif /* NVSTUSB_TIMING_H */
