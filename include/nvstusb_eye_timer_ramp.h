/*
 * Diagnostic SET_EYE command-timer ramp, independent of the startup T2
 * frame reload. This models bytes 4..7 of the 0xAA USB command only.
 *
 * Values are nominal firmware timer units, NOT measured IR or lens times.
 * Keep the pure arithmetic here so it can be unit-tested without libusb.
 */
#ifndef NVSTUSB_EYE_TIMER_RAMP_H
#define NVSTUSB_EYE_TIMER_RAMP_H

#include <stdint.h>

#define NVSTUSB_EYE_TIMER_MIN_US 500.0
#define NVSTUSB_EYE_TIMER_T2_TICKS_PER_US 12.0

static inline double nvstusb_eye_timer_ramped_us(
    double base_us, int64_t elapsed_ns, double rate_us_per_s,
    double ramp_limit_us, double frame_us)
{
    // Caller validates the rate and limit. elapsed_ns is CLOCK_MONOTONIC
    // relative to the FIRST actual SET_EYE command, never to USB setup.
    double delta_us = elapsed_ns > 0 ?
        (double)elapsed_ns / 1e9 * rate_us_per_s : 0.0;
    if (delta_us > ramp_limit_us)
        delta_us = ramp_limit_us;
    if (delta_us < -ramp_limit_us)
        delta_us = -ramp_limit_us;

    double result = base_us + delta_us;
    if (result < NVSTUSB_EYE_TIMER_MIN_US)
        result = NVSTUSB_EYE_TIMER_MIN_US;
    if (result > frame_us - NVSTUSB_EYE_TIMER_MIN_US)
        result = frame_us - NVSTUSB_EYE_TIMER_MIN_US;
    return result;
}

// Preserve the original T2_COUNT floating-point truncation semantics.
// Result is signed 32-bit: callers transmit its bit pattern little-endian.
static inline int32_t nvstusb_eye_timer_command_count(double timer_us)
{
    return (int32_t)(1.0 - timer_us * NVSTUSB_EYE_TIMER_T2_TICKS_PER_US);
}

#endif
