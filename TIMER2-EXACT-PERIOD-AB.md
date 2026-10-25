# Timer 2 period quantization: A/B experiment (experimental)

Branch: `feature/timer2-exact-period-diagnostics`, derived from
`feature/usb-eye-timer-calibration` at
`8291be10ee100f4517c0d29807b8c99e9d570896`.

**Scope:** change only the encoding of the initial Timer 2 `frameTime`
reload (`z` at `nvstusb_set_rate()` / the 0x201b field).
The rate-derived firmware `eye_timer_us` in every `SET_EYE` command, the
fixed `w/x/y` timing profile, `activeTime`, USB frame cadence, and
mpv's MSC scheduler are unchanged.

## Background

The historical formula first truncates a frame period in **microseconds**:

```c
int32_t frameTime = 1000000.0 / rate; // truncates fractions of microsecond
int32_t z = NVSTUSB_T2_COUNT(frameTime); // nominal 12MHz timer clock
```

The opt-in variant rounds only once at the **hardware tick**:

```c
int64_t ticks = llround(12000000.0 / rate);
int32_t z = (int32_t)(1 - ticks);
```

At the earlier reported ~119.998 Hz, the nominal period is
~8333.472 microseconds; historical `z` encodes 8333.000 microseconds,
while opt-in `z` encodes 8333.500 microseconds. This is a ~0.5-us
difference *per theoretical independent firmware period*. If Timer 2 is
free-running and the nominal 12MHz hardware clock is accurate, such a
difference corresponds to about 60 microseconds per second of relative
phase motion. This is **not** a measured lens drift.

The source does not include NVIDIA's proprietary firmware or a verified
interpretation of how each `NVSTUSB_CMD_SET_EYE` packet affects Timer 2.
In particular, we **cannot yet say** if SET_EYE resets its free-running
phase, reschedules IR edges, or merely updates state. Clock oscillator
error, IR timing, optical latency and firmware state may remain relevant.

## Build & arithmetic test (without emitter)

```sh
cd ~/Projects/src/libnvstusb
git fetch origin feature/timer2-exact-period-diagnostics
git switch --track origin/feature/timer2-exact-period-diagnostics
autoreconf -fi
./configure
make -j"$(nproc)"
make check
```

`make check` compiles a stand-alone C test without libusb, KMS or an IR
device. It verifies the historic 120Hz register bytes remain intact and
that tick-accurate rounding is within half a nominal 12MHz timer tick at
several rates, including 119.998Hz.

This development environment has no network access to fetch the complete
repository or the target USB/KMS hardware; the arithmetic formula has
been compiled and checked separately, but a full autotools build and
optical test must run on the Arch target.

## A/B test with unchanged mpv (do not recalibrate during runs)

The old working mpv branch
`feature/drm-stereo-usb-phase-scheduler` can remain on commit
`44736497fa764c47ca2955e6c7d3a43770cd0b0a`.
Rebuild ONLY the changed libnvstusb from this branch and point mpv
at its freshly built `src/.libs/libnvstusb.so`.

**Keep identical** monitor mode, physical USB port, firmware file,
`MPV_DRM_STEREO_USB_PHASE_US`, and calibration pattern for each run.
Do **not** set `NVSTUSB_EYE_TIMER_US` or change the firmware timer.

Do at least two tests, ideally in randomized sequence:
- A, legacy: `NVSTUSB_FRAME_TIMER_EXACT=0` (the default).
- B, precise: `NVSTUSB_FRAME_TIMER_EXACT=1`.

For example, use the existing mpv command with the environment shown:
```sh
env -u NVSTUSB_EYE_TIMER_US \
  NVSTUSB_FRAME_TIMER_EXACT=1 \
  MPV_DRM_STEREO_USB=1 \
  MPV_DRM_STEREO_USB_PHASE_SCHED=1 \
  MPV_DRM_STEREO_USB_PHASE_US=-1600 \
  MPV_DRM_STEREO_USB_LIBRARY=/absolute/path/to/src/.libs/libnvstusb.so \
  MPV_DRM_STEREO_USB_FIRMWARE=/absolute/path/to/nvstusb.fw \
  /absolute/path/to/mpv/build/mpv --no-config --vo=gpu --gpu-context=drm \
    --drm-connector=DP-1 --drm-mode=2560x1440@120 \
    --drm-vrr-enabled=no --drm-stereo-3d=yes \
    --drm-stereo-calibration=before-play \
    --drm-stereo-pacing-eye=right --video-sync=display-resample \
    --log-file=/tmp/mpv-timer2-exact.log \
    /absolute/path/to/movie.mkv
```

Leave the synthetic L-white/R-black calibration image on-screen for
5-10 minutes. Do **not** tweak arrows; note/record the contamination
boundary at start, every 30 seconds, and at the end. Prefer camera or
optical sensor observations rather than prolonged viewing through
unstable shutter glasses. Exit and repeat in the other mode, using
randomized restart intervals. Store separate logs per run.

Log `nvstusb: T2 frame reload: mode=... target=... legacy=... exact=...`
is emitted to terminal **stderr**, not necessarily to mpv `--log-file`
(it comes from libnvstusb). Use `2> /tmp/nvstusb-timer2-A.stderr`
if you want to capture the firmware calculations; it does not redirect
stdout, preserving mpv's usual terminal keyboard control. Confirm the
logged `mode` and `selected_ticks` before comparing runs.

## Interpretation / limits

- Different and repeatable drift slope between A and B supports a role
  for the firmware Timer 2 frame reload / clock accuracy.
- Nearly identical drift in A and B suggests other firmware timing,
  IR/receiver phase acquisition, or host USB hardware timing dominates.
- Sudden phase jumps or missing shutters require a separate time-
  correlated USB/MSC/IR capture; changing Timer 2 arithmetic cannot
  reliably prevent loss of the IR receiver state.
- No apparent drift for either A or B is consistent with good phase
  behavior but does not prove a firmware PLL or a SET_EYE phase reset.

**Do not interpret an arithmetic timing log as measured optical phase.**
