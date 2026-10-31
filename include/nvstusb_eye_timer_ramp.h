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

/*
 * Five-phase, repeating, fixed-r A/B sequence for SAME-RUN optical checks.
 * Slots of step_seconds each: 0, -step_us, 0, +step_us, 0; repeat.
 * A fixed-r step does NOT touch frame reload z or host send deadlines.
 */
static inline int nvstusb_eye_timer_step_slot(int64_t elapsed_ns,
                                              unsigned step_seconds)
{
    if (elapsed_ns < 0 || step_seconds == 0)
        return 0;
    return (int)((elapsed_ns / ((int64_t)step_seconds * 1000000000LL)) % 5);
}

static inline double nvstusb_eye_timer_step_us(double base_us,
                                                int64_t elapsed_ns,
                                                double step_us,
                                                unsigned step_seconds,
                                                double frame_us)
{
    int slot = nvstusb_eye_timer_step_slot(elapsed_ns, step_seconds);
    double value = base_us;
    if (slot == 1)
        value -= step_us;
    else if (slot == 3)
        value += step_us;
    if (value < NVSTUSB_EYE_TIMER_MIN_US)
        value = NVSTUSB_EYE_TIMER_MIN_US;
    if (value > frame_us - NVSTUSB_EYE_TIMER_MIN_US)
        value = frame_us - NVSTUSB_EYE_TIMER_MIN_US;
    return value;
}

#endif
