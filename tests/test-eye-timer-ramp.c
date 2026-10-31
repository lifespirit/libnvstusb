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
          UINT32_C(0xffff26fd)) {
        fprintf(stderr, "FAIL SET_EYE raw 32-bit timer encoding\n");
        return EXIT_FAILURE;
    }

    puts("PASS: bounded per-SET_EYE command timer ramp and register encoding");
    return EXIT_SUCCESS;
}
