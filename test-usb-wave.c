#define _POSIX_C_SOURCE 200809L
/*
 * Standalone USB shutter cadence test: smooth 120 -> 115 -> 120 Hz wave.
 *
 * The emitter firmware is configured ONCE at --base-rate (default 120 Hz).
 * Only the timing of nvstusb_send_eye() calls is modulated, not the
 * firmware's internal timers or any display/monitor refresh mode.
 *
 * Examples:
 *   ./test-usb-wave nvstusb.fw --seconds=30 --cycle=10 --eye=alternate
 *   ./test-usb-wave nvstusb.fw --seconds=15 --cycle=10 --eye=right
 *   ./test-usb-wave --dry-run --seconds=10
 *
 * No display synchronization is attempted. Do NOT wear the glasses.
 */
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "nvstusb.h"

#define NS_PER_SECOND UINT64_C(1000000000)
#define TWO_PI 6.283185307179586476925286766559

static volatile sig_atomic_t running = 1;
static volatile sig_atomic_t stop_signal = 0;

static void interrupt_test(int signo)
{
    stop_signal = signo;
    running = 0;
}

static uint64_t monotonic_ns(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        perror("clock_gettime");
        return 0;
    }
    return (uint64_t)ts.tv_sec * NS_PER_SECOND + (uint64_t)ts.tv_nsec;
}

static struct timespec ns_to_ts(uint64_t ns)
{
    struct timespec ts;
    ts.tv_sec = (time_t)(ns / NS_PER_SECOND);
    ts.tv_nsec = (long)(ns % NS_PER_SECOND);
    return ts;
}

static int parse_number(const char *value, double *result)
{
    char *end = NULL;
    errno = 0;
    const double n = strtod(value, &end);
    if (errno || !end || end == value || *end || !isfinite(n))
        return 0;
    *result = n;
    return 1;
}

/* The derivative is zero at 120 Hz and 115 Hz, unlike a triangle wave. */
static double wave_rate(double elapsed, double high, double low, double cycle)
{
    return low + 0.5 * (high - low) *
           (1.0 + cos(TWO_PI * elapsed / cycle));
}

static void usage(const char *name)
{
    fprintf(stderr,
            "Usage: %s [nvstusb.fw] [--seconds=30] [--cycle=10]\n"
            "       [--max-rate=120] [--min-rate=115] [--base-rate=120]\n"
            "       [--eye=alternate|left|right] [--invert-eyes] [--dry-run]\n"
            "\n"
            "The requested USB command frequency varies smoothly:\n"
            "  120 Hz at t=0, 115 Hz at t=cycle/2, 120 Hz at t=cycle.\n"
            "The emitter firmware is set ONCE at --base-rate (120 Hz).\n"
            "Neither the monitor refresh rate nor firmware period changes.\n"
            "Does NOT synchronize to vblank. Do NOT wear the glasses.\n",
            name);
}

int main(int argc, char **argv)
{
    const char *firmware = "nvstusb.fw";
    double high = 120.0, low = 115.0, base_rate = 120.0;
    double seconds = 30.0, cycle = 10.0;
    enum { ALTERNATE, LEFT_ONLY, RIGHT_ONLY } eye_mode = ALTERNATE;
    int invert = 0, dry_run = 0, firmware_seen = 0;

    for (int i = 1; i < argc; ++i) {
        const char *a = argv[i];
        if (!strncmp(a, "--seconds=", 10)) {
            if (!parse_number(a + 10, &seconds)) goto bad_usage;
        } else if (!strncmp(a, "--cycle=", 8)) {
            if (!parse_number(a + 8, &cycle)) goto bad_usage;
        } else if (!strncmp(a, "--max-rate=", 11)) {
            if (!parse_number(a + 11, &high)) goto bad_usage;
        } else if (!strncmp(a, "--min-rate=", 11)) {
            if (!parse_number(a + 11, &low)) goto bad_usage;
        } else if (!strncmp(a, "--base-rate=", 12)) {
            if (!parse_number(a + 12, &base_rate)) goto bad_usage;
        } else if (!strncmp(a, "--eye=", 6)) {
            const char *v = a + 6;
            if (!strcmp(v, "alternate")) eye_mode = ALTERNATE;
            else if (!strcmp(v, "left")) eye_mode = LEFT_ONLY;
            else if (!strcmp(v, "right")) eye_mode = RIGHT_ONLY;
            else goto bad_usage;
        } else if (!strcmp(a, "--invert-eyes")) {
            invert = 1;
        } else if (!strcmp(a, "--dry-run")) {
            dry_run = 1;
        } else if (!strcmp(a, "--help") || !strcmp(a, "-h")) {
            usage(argv[0]);
            return EXIT_SUCCESS;
        } else if (a[0] != '-' && !firmware_seen) {
            firmware = a;
            firmware_seen = 1;
        } else {
            goto bad_usage;
        }
    }
    if (!(seconds >= 1 && seconds <= 120 &&
          cycle >= 2 && cycle <= 120 &&
          low > 60 && high <= 240 && high >= low &&
          base_rate > 60 && base_rate <= 240))
        goto bad_usage;

    /* nvstusb_init() has legacy GLX detection, but the test never
     * invokes GLX-based nvstusb_swap() or the stereo thread. */
    if (setenv("__GL_SYNC_TO_VBLANK", "1", 1) != 0) {
        perror("setenv");
        return EXIT_FAILURE;
    }
    signal(SIGINT, interrupt_test);
    signal(SIGTERM, interrupt_test);

    struct nvstusb_context *ctx = NULL;
    if (!dry_run) {
        ctx = nvstusb_init(firmware);
        if (!ctx) {
            fprintf(stderr, "FAILED: cannot initialize the USB emitter\n");
            return EXIT_FAILURE;
        }
        nvstusb_set_rate(ctx, (float)base_rate);
        const int init_error = nvstusb_last_error(ctx);
        if (init_error != 0) {
            fprintf(stderr, "FAILED: initial emitter timing setup: %d\n",
                    init_error);
            nvstusb_deinit(ctx);
            return EXIT_FAILURE;
        }
        if (invert)
            nvstusb_invert_eyes(ctx);
    }

    const char *eye_name = eye_mode == LEFT_ONLY ? "left" :
                           eye_mode == RIGHT_ONLY ? "right" : "alternate";
    printf("USB shutter wave: %.3f -> %.3f -> %.3f Hz, cycle %.2fs, "
           "duration %.2fs, eye %s%s%s\n",
           high, low, high, cycle, seconds, eye_name,
           invert ? " inverted" : "", dry_run ? " DRY-RUN" : "");
    printf("Emitter firmware rate: %.3f Hz (configured ONCE); "
           "monitor refresh: UNCHANGED; no video/vblank synchronization\n",
           base_rate);
    printf("t_sec,target_hz,actual_hz,commands,late_gt_2ms,max_deadline_lag_ms,"
           "max_usb_duration_ms\n");
    fflush(stdout);

    const uint64_t start_ns = monotonic_ns();
    if (!start_ns) {
        if (ctx) nvstusb_deinit(ctx);
        return EXIT_FAILURE;
    }
    const uint64_t stop_ns = start_ns + (uint64_t)llround(seconds * 1e9);
    uint64_t deadline_ns = start_ns;
    uint64_t report_start_ns = start_ns;
    uint64_t first_send_ns = 0, last_send_ns = 0;
    uint64_t max_lag_ns = 0, max_usb_ns = 0;
    unsigned sent = 0, window_sent = 0, late = 0, window_late = 0;
    int error = 0;
    while (running && deadline_ns < stop_ns) {
        const struct timespec until = ns_to_ts(deadline_ns);
        int sleep_err;
        do {
            sleep_err = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME,
                                        &until, NULL);
        } while (sleep_err == EINTR && running);
        if (!running) break;
        if (sleep_err) {
            fprintf(stderr, "FAILED: clock_nanosleep: %s\n",
                    strerror(sleep_err));
            error = sleep_err;
            break;
        }

        const uint64_t before = monotonic_ns();
        if (!before) { error = EIO; break; }
        const uint64_t lag = before > deadline_ns ? before - deadline_ns : 0;
        if (lag > max_lag_ns) max_lag_ns = lag;
        if (lag > UINT64_C(2000000)) {
            ++late;
            ++window_late;
        }

        enum nvstusb_eye eye =
            eye_mode == LEFT_ONLY ? nvstusb_left :
            eye_mode == RIGHT_ONLY ? nvstusb_right :
            (sent & 1u) ? nvstusb_right : nvstusb_left;

        if (!dry_run) {
            error = nvstusb_send_eye(ctx, eye);
            if (error != 0) {
                fprintf(stderr, "FAILED: USB command %u: %d\n",
                        sent + 1, error);
                break;
            }
        }
        const uint64_t after = monotonic_ns();
        if (!after) { error = EIO; break; }
        if (!first_send_ns) first_send_ns = after;
        last_send_ns = after;
        ++sent;
        ++window_sent;
        if (after - before > max_usb_ns)
            max_usb_ns = after - before;

        if (after >= report_start_ns + NS_PER_SECOND) {
            const double elapsed = (double)(after - start_ns) * 1e-9;
            const double window_sec =
                (double)(after - report_start_ns) * 1e-9;
            const double observed = window_sec > 0.0 ?
                                    window_sent / window_sec : 0.0;
            printf("%.3f,%.3f,%.3f,%u,%u,%.3f,%.3f\n",
                   elapsed, wave_rate(elapsed, high, low, cycle),
                   observed, window_sent, window_late,
                   max_lag_ns / 1e6, max_usb_ns / 1e6);
            fflush(stdout);
            report_start_ns = after;
            window_sent = 0;
            window_late = 0;
            max_lag_ns = 0;
            max_usb_ns = 0;
        }

        /* Integrate the requested instantaneous frequency over time.
         * Absolute deadlines avoid sleep accumulation. Intentionally do
         * not retune nvstusb_set_rate(): that would change the firmware
         * and test a different phenomenon.
         */
        const double t = (double)(deadline_ns - start_ns) * 1e-9;
        const double hz = wave_rate(t, high, low, cycle);
        uint64_t step_ns = (uint64_t)llround(1e9 / hz);
        if (!step_ns) step_ns = 1;
        deadline_ns += step_ns;
        /* If USB or scheduling is over a whole period late, don't
         * send a catch-up burst (that is an independent test case).
         * Resume at the next future deadline without changing eye
         * parity except by actual successful USB commands. */
        if (deadline_ns <= after) {
            const uint64_t missed = (after - deadline_ns) / step_ns + 1;
            deadline_ns += missed * step_ns;
            fprintf(stderr, "WARNING: missed %" PRIu64
                    " command deadlines; no catch-up burst\n", missed);
        }
    }

    const double elapsed = first_send_ns && last_send_ns >= first_send_ns ?
        (double)(last_send_ns - first_send_ns) * 1e-9 : 0.0;
    const double avg = sent > 1 && elapsed > 0 ? (sent - 1) / elapsed : 0.0;
    printf("SUMMARY: commands=%u actual_avg=%.3fHz late_gt_2ms=%u "
           "duration=%.3fs%s\n", sent, avg, late, elapsed,
           dry_run ? " DRY-RUN" : "");
    if (eye_mode == ALTERNATE)
        printf("Estimated per-eye repetition: %.3fHz on average\n", avg / 2.0);
    printf("NOTE: success is USB submission only; no optical state, "
           "IR timing or display synchronization measured.\n");
    if (ctx) nvstusb_deinit(ctx);
    if (error) return EXIT_FAILURE;
    if (stop_signal) return 128 + (int)stop_signal;
    puts("PASS: wave test completed without reported USB errors");
    return EXIT_SUCCESS;

bad_usage:
    usage(argv[0]);
    return EXIT_FAILURE;
}
