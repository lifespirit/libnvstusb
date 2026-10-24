# NVIDIA 3D Vision USB test under Wayland (experimental)

This repository imports libnvstusb from the SourceForge SVN r36 source tree
and tracks experimental Arch Linux / KDE Wayland improvements.

Upstream: https://sourceforge.net/p/libnvstusb/code/HEAD/tree/

## Tested hardware and limitations

- NVIDIA 3D Vision USB IR emitter (USB ID `0955:0007`)
- Samsung Odyssey G7 monitor; AMD/Intel GPU; Arch Linux with KDE Wayland
- On October 17, 2026 the USB-only test passed **four consecutive 600/600
  bulk OUT shutter-command runs** on Arch Linux with the emitter connected
  directly to the laptop USB port. The first run included firmware loading
  and re-enumeration; the next three runs reused the initialized device.
- Previously, with the emitter behind nested USB hubs, the test failed on
  endpoint 0x01 with `LIBUSB_ERROR_IO (-1)` and timeouts. Kernel logs on the
  problematic connection also showed USB descriptor read errors (-110)
  and re-enumeration failures. This strongly implicates the USB connection
  path, but the exact cause remains unknown.
- Initial glasses darkening was observed, but **the shutter timing/IR output
  has not been verified against a display**.
- This USB test **does not synchronize** the shutters with rendered frames.
  SBS/OU 3D movie playback is a separate future step.

## Build on Arch Linux

```sh
sudo pacman -S --needed base-devel git autoconf automake libtool pkgconf \
  libusb devil freeglut libxxf86vm libx11 libglvnd mesa

autoreconf -fi
./configure --prefix=/usr
make -j"$(nproc)"

# The standalone USB test is built automatically as ./test-usb
# Confirm libusb is linked directly into the shared library:
readelf -d src/.libs/libnvstusb.so | grep NEEDED
```

No `sudo make install` is needed to run `./test-usb` from the repository.

The legacy `example/3dv.c` demo is kept in source control but excluded from
default builds. On recent Arch Linux, ILUT fails to provide
`ilutGLBindTexImage` and `ilutGLBuildMipmaps`.

The `configure.ac` version requirements for IL/ILU/ILUT and GL were removed
because the Arch DevIL `.pc` metadata may expose empty Version fields.

## Firmware

The emitter needs proprietary NVIDIA firmware `nvstusb.fw` after power-on.
It is **not included** in this repository. Obtain and extract it from a
driver you are entitled to use. The repository includes the original
`tools/nvstusb-extractfw` utility, for use with the Windows `nvstusb.sys`.

Place `nvstusb.fw` in the repository root before running the test.

## USB permissions

```sh
sudo install -m 0644 udev/70-nvidia-3dvision.rules \
  /etc/udev/rules.d/70-nvidia-3dvision.rules
sudo udevadm control --reload-rules
```

Physically unplug and reconnect the USB emitter to apply the rule.
For `TAG+="uaccess"`, run from your active local graphical session.

## USB-only test

```sh
timeout -s TERM -k 2s 15s ./test-usb
```

The program internally chooses libnvstusb's no-GLX path by setting
`__GL_SYNC_TO_VBLANK=1`. It sends 600 alternating left/right shutter
commands with approximately 8.33 ms spacing. It does not open a graphical
window or use X11. Its 120 Hz timing is not phase-locked to monitor vblank.

The diagnostic exits nonzero immediately after the first failed USB OUT transfer, and does not report success when stopped by a timeout. The output includes the interface endpoint addresses.\n\nIf a run hangs or exits with USB errors, collect:

```sh
timeout -s KILL 15s strace -f -ttt -T -yy -s 128 \
  -e trace=ioctl,openat,close,poll,ppoll,read,write \
  -o /tmp/nvstusb.strace ./test-usb
journalctl -k --since "15 minutes ago"
```

Do not run multiple instances simultaneously. If firmware re-enumeration
fails, unplug and reconnect the emitter before retrying. Most USB writes now
have a 1-second timeout; firmware control writes use a 2-second timeout.

## Upstream and license

This is a **snapshot import, not a conversion of the full SVN history**.
Files came from a public SourceForge-derived GitHub mirror with 2017 revisions
reconstructed from SourceForge SVN r36 file views. The original license and
copyright headers have been retained; see `COPYING`.

No NVIDIA firmware or Windows binaries are stored in Git.

## Direct shutter API for native Wayland applications

`nvstusb_send_eye(ctx, nvstusb_left | nvstusb_right)` sends a shutter
command immediately without GLX, OpenGL buffer swaps, or the
`__GL_SYNC_TO_VBLANK` environment workaround. The return value is the
first latched USB OUT transfer error (0 on success). It requires successful
`nvstusb_set_rate(ctx, refresh_rate)` first. Frame pacing and presentation
feedback are the calling application's responsibility.


## Isolate the glasses from Bino (Oct 2026)

The IR glasses were observed to lose shutter activation almost completely
at 144 Hz when controlled by Bino; at 120 Hz, the two lenses appeared
to shutter differently. **Do not attribute this to dropped video frames
before checking the emitter itself.**

The old firmware timing profile in \`src/nvstusb.c\` sets a fixed
timer-X interval of 4.77425 ms and an active interval of 2.080 ms,
while deriving only the frame-reload interval from \`rate\`. The sum
(6.85425 ms) leaves about 1.479 ms inside an 8.333 ms period at 120 Hz
but just 0.090 ms inside a 6.944 ms period at 144 Hz. These timers'
exact firmware interpretation is partly reverse-engineered; the
arithmetic is a clue, **not proof of a specific failure mechanism**.

### Direct left/right/alternating diagnostic

Rebuild after updating to the revised \`test-usb\`, then use the emitter
directly connected to a USB port, with Bino closed:

\`\`\`sh
make -j"$(nproc)"
./test-usb nvstusb.fw --rate=120 --seconds=15 --eye=left
./test-usb nvstusb.fw --rate=120 --seconds=15 --eye=right
./test-usb nvstusb.fw --rate=120 --seconds=15 --eye=alternate
\`\`\`

Visually compare left and right shutter behavior independently, preferably
recorded in slow motion. This tests USB/firmware/IR and the glasses **without
Qt, KWin, video decoding, or frame-swap callbacks**. It does not test
actual synchronization with monitor scanout. USB \`PASS\` only verifies
commands reached libusb without reported OUT transfer errors; it does not
verify emitted infrared pulses or received shutter timings.

Do not chase 144/240 Hz in Bino before the 120 Hz standalone diagnostic
is stable. Faster refresh rates need a properly measured and calibrated
emitter shutter timing profile, not merely a different frame-rate number.


## Experimental firmware eye timer override for optical calibration (2026-10-25)

**Branch:** `feature/usb-eye-timer-calibration`; opt-in, unverified
against actual lens waveform. Default behavior remains unchanged.

Native DRM 3D playback at 120 Hz with 100% confirmed KMS
flips and 100% successful `nvstusb_send_eye()` USB
bulk transfers can still display a screen-height-dependent
L/R leakage stripe through the glasses. On one tested LCD
the user observed three run-to-run states: apparently
correct, a small contaminated region at the top, or
approximately half the vertical image affected. The
hardware refreshes a raster from top to bottom while
both shutters act globally; variable emitter firmware
timing, shutter acquisition phase, LCD pixel response
and scanout position must be investigated, not inferred
from USB transport success.

The existing `nvstusb_set_eye()` encodes its fourth
argument as a 32-bit Timer-2 reload field:

```c
r = NVSTUSB_T2_COUNT((1e6 / ctx->rate) / 1.8);
```

At 120Hz this is a **nominal 4629.63 µs** timer
parameter encoded at 12 MHz; this is *not* proof that
the lens opens at 4629.63µs. Its actual meaning inside
the proprietary firmware is only partially documented.
The separate fixed `nvstusb_set_rate()` waveform
profile, including a 4774.25µs timer and a 2080µs
active interval, is *not* modified by this option.

Use `NVSTUSB_EYE_TIMER_US` to A/B sweep just this
one command parameter:

```sh
git fetch origin feature/usb-eye-timer-calibration
git switch feature/usb-eye-timer-calibration
# or git switch --track origin/feature/usb-eye-timer-calibration
make -j"$(nproc)"

# mpv dynamically loads THIS freshly built .so:
NVSTUSB_EYE_TIMER_US=3500 \
MPV_DRM_STEREO_USB_LIBRARY=/absolute/path/to/libnvstusb/src/.libs/libnvstusb.so \
... ./build/mpv --drm-stereo-3d=yes ...
```

The allowed range is 500µs through `(1000000/rate)-500`
µs (at 120Hz, 500..7833). Invalid values warn and
preserve the original timer. If the variable is unset,
the original rate-based formula is used exactly.
Suggested *diagnostic only* values are 3500, 4500
and 5500µs, with several **independent** launches for
each and no other phase / inversion / display settings
changed. Observe through lenses with a camera or optical
sensor, not by wearing them during calibration. The
library prints the requested/accepted setting to stderr;
it does not establish IR or optical delivery timing.

If the vertical boundary moves predictably with the
parameter, the firmware command timer contributes to
optical phase. If runs with the exact same parameter
still show substantially different phase, investigate
independent firmware/receiver acquisition, clocks,
command-to-IR conversion and LCD response. A constant
timer value cannot necessarily fix a run-to-run
acquisition or phase-locking problem. Never compensate
an unknown display-to-lens offset by blocking the
real-time KMS page-flip thread.


### Effective timer verification after four 3500us optical runs

Four launches with `NVSTUSB_EYE_TIMER_US=3500` on the target
i915 120Hz display produced widely varying optical
boundary locations while all KMS flip and USB OUT
commands succeeded, with zero losses. This demonstrates
that the timer alone does not give reproducible
optical phase across launches. The mpv log showed
the requested env value but did not yet verify
what the loaded `libnvstusb.so` actually accepted.

This calibration branch now exports:

```c
int nvstusb_effective_eye_timer_us(struct nvstusb_context *ctx);
```

This returns the actual command timer field in
integer microseconds selected by the library after
`nvstusb_set_rate`. Without override, it returns the
legacy formula (rate-based); before set-rate it
returns -1. **This is a code-level readback, NOT
a measurement of USB-to-IR delay or lens open time.**

On the updated mpv branch, requesting an override
requires this getter and fails VO init rather than
silently using a different/older library. The log
must contain:

```text
Stereo USB: verified firmware eye timer=3500 us (requested=3500, phase NOT optically calibrated)
```

Rebuild the library and mpv, point mpv explicitly
to this branch's rebuilt `.so`, then repeat
optical tests with interleaved timer values.
