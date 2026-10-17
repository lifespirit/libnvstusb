#define _POSIX_C_SOURCE 200809L
/*
 * USB-only NVIDIA 3D Vision shutter test.
 * No display synchronization (GLX, Wayland, or otherwise) is attempted.
 *
 * Example:
 *   ./test-usb nvstusb.fw --rate=120 --seconds=15 --eye=alternate
 *   ./test-usb nvstusb.fw --rate=120 --seconds=15 --eye=left
 *   ./test-usb nvstusb.fw --rate=120 --seconds=15 --eye=right
 *   ./test-usb nvstusb.fw --rate=120 --seconds=15 --eye=right --invert-eyes
 */
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "nvstusb.h"

static volatile sig_atomic_t running = 1;
static volatile sig_atomic_t stopped_by_signal = 0;

static void stop_test(int signo)
{
    stopped_by_signal = signo;
    running = 0;
}

static void usage(const char *prog)
{
    fprintf(stderr,
        "Usage: %s [nvstusb.fw] [--rate=120] [--seconds=5] "
        "[--eye=alternate|left|right] [--invert-eyes]\n"
        "The display is NOT synchronized. Observe each lens directly.\n",
        prog);
}

int main(int argc, char **argv)
{
    const char *firmware = "nvstusb.fw";
    float rate = 120.0f;
    unsigned int seconds = 5;
    enum { ALTERNATE, LEFT_ONLY, RIGHT_ONLY } mode = ALTERNATE;
    int have_firmware = 0;
    int invert_eyes = 0;

    for (int a = 1; a < argc; ++a) {
        if (!strncmp(argv[a], "--rate=", 7)) {
            char *end;
            rate = strtof(argv[a] + 7, &end);
            if (*end || !(rate > 60.0f && rate <= 240.0f)) {
                usage(argv[0]);
                return EXIT_FAILURE;
            }
        } else if (!strncmp(argv[a], "--seconds=", 10)) {
            char *end;
            unsigned long n = strtoul(argv[a] + 10, &end, 10);
            if (*end || n < 1 || n > 300) {
                usage(argv[0]);
                return EXIT_FAILURE;
            }
            seconds = (unsigned int)n;
        } else if (!strncmp(argv[a], "--eye=", 6)) {
            const char *value = argv[a] + 6;
            if (!strcmp(value, "alternate"))
                mode = ALTERNATE;
            else if (!strcmp(value, "left"))
                mode = LEFT_ONLY;
            else if (!strcmp(value, "right"))
                mode = RIGHT_ONLY;
            else {
                usage(argv[0]);
                return EXIT_FAILURE;
            }
        } else if (!strcmp(argv[a], "--invert-eyes")) {
            invert_eyes = 1;
        } else if (!strcmp(argv[a], "--help") || !strcmp(argv[a], "-h")) {
            usage(argv[0]);
            return EXIT_SUCCESS;
        } else if (argv[a][0] == '-' || have_firmware) {
            usage(argv[0]);
            return EXIT_FAILURE;
        } else {
            firmware = argv[a];
            have_firmware = 1;
        }
    }

    /*
     * Keep this test free of legacy GLX calls even if the library
     * detected GLX_SGI_video_sync on this machine.
     */
    if (setenv("__GL_SYNC_TO_VBLANK", "1", 1) != 0) {
        perror("setenv");
        return EXIT_FAILURE;
    }

    signal(SIGINT, stop_test);
    signal(SIGTERM, stop_test);

    struct nvstusb_context *ctx = nvstusb_init(firmware);
    if (!ctx) {
        fprintf(stderr, "Failed to initialize 3D Vision emitter\n");
        return EXIT_FAILURE;
    }

    nvstusb_set_rate(ctx, rate);
    int error = nvstusb_last_error(ctx);
    if (error != 0) {
        fprintf(stderr, "FAILED: USB initialization write returned %d\n", error);
        nvstusb_deinit(ctx);
        return EXIT_FAILURE;
    }

    if (invert_eyes)
        nvstusb_invert_eyes(ctx);

    if (rate > 120.0f) {
        fprintf(stderr,
            "WARNING: libnvstusb's legacy shutter timing profile has fixed "
            "4.5685/4.77425/2.080 ms parameters and is not validated "
            "at %.3f Hz.\n", rate);
    }

    const char *mode_name = mode == LEFT_ONLY ? "left"
            : mode == RIGHT_ONLY ? "right" : "alternate";
    const int total = (int)(rate * seconds + 0.5f);
    const long period_ns = (long)(1000000000.0 / rate + 0.5);
    printf("3D Vision USB: rate %.3f Hz, eye %s%s, duration %u s, "
           "%d commands (NOT display-synchronized)\n",
           rate, mode_name, invert_eyes ? " (inverted)" : "", seconds, total);
    fflush(stdout);

    struct timespec deadline;
    if (clock_gettime(CLOCK_MONOTONIC, &deadline) != 0) {
        perror("clock_gettime");
        nvstusb_deinit(ctx);
        return EXIT_FAILURE;
    }

    int sent = 0;
    struct timespec first_send = {0, 0}, last_send = {0, 0};
    for (int i = 0; i < total && running; ++i) {
        enum nvstusb_eye eye = mode == LEFT_ONLY ? nvstusb_left
                : mode == RIGHT_ONLY ? nvstusb_right
                : (i & 1) ? nvstusb_right : nvstusb_left;

        error = nvstusb_send_eye(ctx, eye);
        if (error != 0) {
            fprintf(stderr, "FAILED: shutter transfer %d/%d: USB error %d\n",
                    i + 1, total, error);
            break;
        }
        /* Timestamp successful USB submissions. This excludes the final
         * sleep and reports observed cadence rather than nominal rate. */
        if (clock_gettime(CLOCK_MONOTONIC, &last_send) != 0) {
            perror("clock_gettime");
            error = -1;
            break;
        }
        if (sent == 0)
            first_send = last_send;
        ++sent;

        if (sent % 120 == 0) {
            printf("Sent %d/%d shutter commands\n", sent, total);
            fflush(stdout);
        }

        deadline.tv_nsec += period_ns;
        if (deadline.tv_nsec >= 1000000000L) {
            deadline.tv_nsec -= 1000000000L;
            ++deadline.tv_sec;
        }
        int sleep_error;
        do {
            sleep_error = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME,
                                         &deadline, NULL);
        } while (sleep_error == EINTR && running);

        if (sleep_error && sleep_error != EINTR) {
            fprintf(stderr, "FAILED: clock_nanosleep: %s\n",
                    strerror(sleep_error));
            error = -1;
            break;
        }
    }

    nvstusb_deinit(ctx);

    if (sent > 1) {
        const double elapsed = (last_send.tv_sec - first_send.tv_sec) +
                (last_send.tv_nsec - first_send.tv_nsec) / 1e9;
        if (elapsed > 0.0) {
            const double actual_hz = (sent - 1) / elapsed;
            printf("Actual USB command rate: %.2f Hz (%d commands, %.3f s)\n",
                   actual_hz, sent, elapsed);
            if (mode == ALTERNATE)
                printf("Estimated per-eye repetition rate: %.2f Hz\n",
                       actual_hz / 2.0);
        }
    }

    if (error != 0)
        return EXIT_FAILURE;
    if (stopped_by_signal != 0) {
        fprintf(stderr, "INTERRUPTED: signal %d after %d commands\n",
                (int)stopped_by_signal, sent);
        return 128 + (int)stopped_by_signal;
    }
    if (sent != total) {
        fprintf(stderr, "FAILED: only %d/%d commands completed\n", sent, total);
        return EXIT_FAILURE;
    }

    printf("PASS: %d USB shutter commands submitted "
           "(NOT display-synchronized)\n", sent);
    return EXIT_SUCCESS;
}
