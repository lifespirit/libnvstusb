/* Standalone, no USB/firmware needed. make check */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "nvstusb_timing.h"

static int check(float rate)
{
    const double target_us = 1000000.0 / (double)rate;
    const int32_t legacy = nvstusb_frame_reload_legacy(rate);
    const int32_t exact = nvstusb_frame_reload_exact(rate);
    const double legacy_us = nvstusb_frame_reload_period_us(legacy);
    const double exact_us = nvstusb_frame_reload_period_us(exact);
    const double max_tick_error_us =
        500000.0 / (double)NVSTUSB_TIMER2_HZ + 1e-9;

    if (fabs(exact_us - target_us) > max_tick_error_us) {
        fprintf(stderr, "FAIL rate=%.8fHz exact error %.6fus exceeds "
                "half a 12MHz tick %.6fus\n", (double)rate,
                fabs(exact_us - target_us), max_tick_error_us);
        return 1;
    }
    if (fabs(exact_us - target_us) >
        fabs(legacy_us - target_us) + 1e-9) {
        fprintf(stderr, "FAIL rate=%.8fHz exact mode worsens rounding\n",
                (double)rate);
        return 1;
    }
    printf("rate=%.6fHz legacy_ticks=%lld exact_ticks=%lld "
           "target_us=%.6f legacy_us=%.6f exact_us=%.6f "
           "legacy_model_drift_us_s=%+.3f exact_model_drift_us_s=%+.3f\n",
           (double)rate, (long long)(1 - (int64_t)legacy),
           (long long)(1 - (int64_t)exact),
           target_us, legacy_us, exact_us,
           (legacy_us - target_us) * rate,
           (exact_us - target_us) * rate);
    return 0;
}

int main(void)
{
    const float rates[] = {60.001f, 80.0f, 100.0f, 119.998f,
                           120.0f, 120.001f, 144.0f, 165.0f, 240.0f};
    for (size_t i = 0; i < sizeof(rates) / sizeof(rates[0]); i++)
        if (check(rates[i]))
            return EXIT_FAILURE;

    // A 120 Hz legacy calculation must remain bit-for-bit compatible with
    // NVSTUSB_T2_COUNT((int32_t)(1000000.0/120.0)): 8333us * 12 ticks/us.
    if (nvstusb_frame_reload_legacy(120.0f) != -99995 ||
        nvstusb_frame_reload_exact(120.0f) != -99999) {
        fputs("FAIL: unexpected 120 Hz legacy/exact reload values\n", stderr);
        return EXIT_FAILURE;
    }
    puts("PASS: nominal Timer2 frame reload A/B computations");
    return EXIT_SUCCESS;
}
