#define _POSIX_C_SOURCE 200809L
/*
 * USB-only NVIDIA 3D Vision shutter test.
 * No Wayland or display synchronization is attempted.
 */
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "nvstusb.h"

static volatile sig_atomic_t running = 1;

static void stop_test(int signo)
{
    (void)signo;
    running = 0;
}

int main(int argc, char **argv)
{
    const char *firmware = argc > 1 ? argv[1] : "nvstusb.fw";

    /* Select libnvstusb's non-GLX swap mode for USB-only testing. */
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

    nvstusb_set_rate(ctx, 120.0f);
    puts("3D Vision USB initialized at 120 Hz (NOT display-synchronized)");
    fflush(stdout);

    const long period_ns = 8333333L;
    struct timespec deadline;
    clock_gettime(CLOCK_MONOTONIC, &deadline);

    for (int i = 0; i < 600 && running; ++i) {
        nvstusb_swap(ctx, (i & 1) ? nvstusb_right : nvstusb_left, NULL);

        if ((i + 1) % 120 == 0) {
            printf("Sent %d/600 shutter commands\n", i + 1);
            fflush(stdout);
        }

        deadline.tv_nsec += period_ns;
        if (deadline.tv_nsec >= 1000000000L) {
            deadline.tv_nsec -= 1000000000L;
            ++deadline.tv_sec;
        }
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &deadline, NULL);
    }

    nvstusb_deinit(ctx);
    puts("Test finished");
    return EXIT_SUCCESS;
}
