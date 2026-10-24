/* nvstusb.h Copyright (C) 2010 Bjoern Paetzel
 *
 * This program comes with ABSOLUTELY NO WARRANTY.
 * This is free software, and you are welcome to redistribute it
 * under certain conditions. See the file COPYING for details
 * */

struct nvstusb_context;

enum nvstusb_eye {
  nvstusb_left = 0,
  nvstusb_right,
  nvstusb_quad,
};

struct nvstusb_keys {
  char deltaWheel;
  char pressedDeltaWheel;
  int  toggled3D;
};

struct nvstusb_context *nvstusb_init(char const * fw);
void nvstusb_deinit(struct nvstusb_context *ctx);
/* Returns the first USB bulk OUT error since initialization (0 on success). */
int nvstusb_last_error(struct nvstusb_context *ctx);
void nvstusb_set_rate(struct nvstusb_context *ctx, float rate);
/* Effective integer Timer-2 command parameter (microseconds) sent for each
 * eye. This does NOT measure physical IR emission/lens latency. Returns -1
 * if the refresh rate has not yet been configured. */
int nvstusb_effective_eye_timer_us(struct nvstusb_context *ctx);
/* Send a shutter command directly, without using any GLX/VBlank method.
 * Intended for external presentation schedulers (Wayland, etc.).
 * Returns 0 on success or a negative libusb error.
 */
int nvstusb_send_eye(struct nvstusb_context *ctx, enum nvstusb_eye eye);
void nvstusb_swap(struct nvstusb_context *ctx, enum nvstusb_eye eye, void (*swapfunc)());
void nvstusb_get_keys(struct nvstusb_context *ctx, struct nvstusb_keys *keys);
void nvstusb_invert_eyes(struct nvstusb_context *ctx);
void nvstusb_start_stereo_thread(struct nvstusb_context *ctx);
void nvstusb_stop_stereo_thread(struct nvstusb_context *ctx);
