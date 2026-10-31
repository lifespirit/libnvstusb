/* Standalone per-SET_EYE command-timer ramp test, no USB hardware. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "nvstusb_eye_timer_ramp.h"

static void near(double actual, double wanted, const char *label)
{
    if (fabs(actual - wanted) > 0.000001) {
        fprintf(stderr, "FAIL %s: actual %.8f wanted %.8f\n",
                label, actual, wanted);
        exit(EXIT_FAILURE);
    }
}

int main(void)
{
    const double frame = 1000000.0 / 120.0;
    const double base = 4629.0;
    const int64_t s = 1000000000LL;

    near(nvstusb_eye_timer_ramped_us(base, 0, -9, 500, frame),
         base, "first command starts with base");
    near(nvstusb_eye_timer_ramped_us(base, 10 * s, -9, 500, frame),
         4539.0, "negative drift ten seconds");
    near(nvstusb_eye_timer_ramped_us(base, 10 * s, 9, 500, frame),
         4719.0, "positive drift ten seconds");
    near(nvstusb_eye_timer_ramped_us(base, 3 * s, -9, 500, frame),
         4602.0, "one microsecond per ~111ms");
    near(nvstusb_eye_timer_ramped_us(base, 100 * s, -9, 500, frame),
         4129.0, "negative limit");
    near(nvstusb_eye_timer_ramped_us(base, 100 * s, 9, 500, frame),
         5129.0, "positive limit");
    near(nvstusb_eye_timer_ramped_us(base, -1, 9, 500, frame),
         base, "monotonic backward guard");
    near(nvstusb_eye_timer_ramped_us(520.0, 10 * s, -9, 500, frame),
         500.0, "firmware lower guard");
    near(nvstusb_eye_timer_ramped_us(frame - 510.0, 10 * s, 9, 500, frame),
         frame - 500.0, "firmware upper guard");
    near(nvstusb_eye_timer_ramped_us(base, 100 * s, 0, 500, frame),
         base, "zero rate never changes r");

    if (nvstusb_eye_timer_command_count(4629.0) != -55547 ||
        nvstusb_eye_timer_command_count(4630.0) != -55559 ||
        (uint32_t)nvstusb_eye_timer_command_count(4629.0) !=
          UINT32_C(0xffff2705)) {
        fprintf(stderr, "FAIL SET_EYE raw 32-bit timer encoding\n");
        return EXIT_FAILURE;
    }

    /* In a single continuous run: baseline, -500, baseline, +500,
     * baseline; then repeat. Boundary checks must be half-open. */
    const int64_t ns = INT64_C(1000000000);
    const unsigned window_s = 20;
    const double step_base = 4629.0;
    const double step_delta = 500.0;
    for (int k = 0; k < 5; k++) {
        const double expected[] = {4629, 4129, 4629, 5129, 4629};
        int64_t t = (int64_t)k * window_s * ns;
        if (nvstusb_eye_timer_step_slot(t, window_s) != k) {
            fprintf(stderr, "FAIL: SET_EYE step slot at %d s\n",
                    k * (int)window_s);
            return EXIT_FAILURE;
        }
        near(nvstusb_eye_timer_step_us(step_base, t,
                                      step_delta, window_s, frame),
             expected[k], "step A/B boundary");
        near(nvstusb_eye_timer_step_us(step_base, t + window_s * ns - 1,
                                      step_delta, window_s, frame),
             expected[k], "step A/B held value");
    }
    near(nvstusb_eye_timer_step_us(step_base, 100 * ns, step_delta,
                                  window_s, frame),
         step_base, "step A/B repeat at 100 s");
    near(nvstusb_eye_timer_step_us(step_base, 120 * ns, step_delta,
                                  window_s, frame),
         step_base - step_delta, "step A/B repeat negative at 120 s");
    near(nvstusb_eye_timer_step_us(step_base, -10, step_delta,
                                  window_s, frame),
         step_base, "step A/B negative time");
    near(nvstusb_eye_timer_step_us(520.0, 20 * ns, step_delta,
                                  window_s, frame),
         500.0, "step A/B lower bound");

    puts("PASS: bounded per-SET_EYE command timer ramp, step sequence and register encoding");
    return EXIT_SUCCESS;
}
