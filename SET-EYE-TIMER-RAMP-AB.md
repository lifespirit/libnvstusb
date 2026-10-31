# NVIDIA SET_EYE firmware-command timer ramp A/B

Branch: `feature/set-eye-timer-ramp-diagnostics` of `lifespirit/libnvstusb`,
based on `feature/timer2-tick-trim-diagnostics`.

## What this tests — and what it does not

The original NVIDIA 8051 firmware recognizes USB bulk endpoint 1
`SET_EYE` commands (opcode `0xaa`, eye `0xff` or `0xfe`) and loads
their last four little-endian bytes into its timer-handling routine
(`0x05ec`). That routine can stop and restart Timer 2 at certain
branches, but not on every command. This parameter is called `r`.

This experiment varies **only** that 32-bit `r` value on each subsequent
`SET_EYE` command, while leaving the initial Timer 2 frame reload
`z` at XDATA `0x201b` and mpv's KMS and host USB deadlines untouched.

The historical / default `r` is the signed 32-bit 12 MHz T2 count
corresponding to roughly `(1e6 / rate) / 1.8` microseconds (about
4629us at 120Hz). `NVSTUSB_EYE_TIMER_US` was already available as a
**constant**, integer-valued `r` override; this branch adds a
**time-dependent** independently testable `r` ramp. The ramp value is
not the physical shutter period, nor is it a direct setting of lens
phase. Do NOT assume a +9us/s command ramp equals +9us/s IR phase drift.

## New opt-in controls

- `NVSTUSB_EYE_TIMER_RAMP_US_PER_S`: signed command-timer ramp in
  microseconds/second, float in -20..+20; unset or 0 disables the
  feature and retains legacy `SET_EYE` packet bytes and arithmetic.
- `NVSTUSB_EYE_TIMER_RAMP_LIMIT_US`: maximum *absolute difference*
  of the command timer from its constant baseline; range 1..2000,
  default 500 microseconds. Once reached, the command timer remains
  constant. Absolute permitted command-timer values are also clamped
  to 500us..(frame_us-500us).
- `NVSTUSB_EYE_TIMER_US`: existing optional base command-timer
  override, e.g. `4629`. Unset uses the historic rate-derived
  value. This is independent of the ramp and is *not required*.
- `NVSTUSB_FRAME_TIMER_EXACT=1` and
  `NVSTUSB_FRAME_TIMER_TICK_BIAS=0`: existing startup T2 reload
  configuration. Keep it constant across all r-ramp A/B runs.

`nvstusb_set_rate()` validates these values. When enabled, the first
actual eye command starts the ramp's monotonic clock (USB initialization
time is excluded). The USB sender samples CLOCK_MONOTONIC for each
command and applies the bounded adjustment to the four-byte r count.
There is **no printing or file I/O per command**. The base-rate setter
and run-end deinitializer print diagnostics to stderr outside the
critical USB loop. A special header contains the pure arithmetic and
`make check` tests it without any USB hardware.

Existing `nvstusb_set_eye_timer_us()` changes the *base* if called at
runtime; the ramp still starts at the first command's monotonic time.

## Build

```sh
cd ~/Projects/src/libnvstusb
git fetch origin feature/set-eye-timer-ramp-diagnostics
git switch --track origin/feature/set-eye-timer-ramp-diagnostics
autoreconf -fi
./configure
make -j"$(nproc)"
make check
```

When the branch is already checked out, simply `git pull --ff-only`.
This still requires the usual libusb, OpenGL and X11 build dependencies.
`make check` runs both `test-frame-timer` and
`test-eye-timer-ramp`.

Point mpv to the *just built* shared library to avoid accidentally
testing the system-wide older version:

```sh
export MPV_DRM_STEREO_USB_LIBRARY="$HOME/Projects/src/libnvstusb/src/.libs/libnvstusb.so"
```

## Same-run step A/B: isolate initial-phase variability

Fixed-r restarts gave 4129us -> ~2:20–2:30 and 5129us ->
~2:00–2:10 to lose the 0% mark, comparable to the 4629us control
(~2:20), despite the previous *ramping* experiments showing larger
differences. Because starting phase and timer origin vary across restarts,
the earlier results do not establish an optical phase correction.

An opt-in same-run diagnostic changes the command's **r** value
without restarting mpv or reinitializing the transmitter:

```sh
export NVSTUSB_EYE_TIMER_STEP_US=500
export NVSTUSB_EYE_TIMER_STEP_SECONDS=20
unset NVSTUSB_EYE_TIMER_RAMP_US_PER_S
unset NVSTUSB_EYE_TIMER_US
unset MPV_DRM_STEREO_USB_FIXED_EYE_TIMER_TEST

export NVSTUSB_FRAME_TIMER_EXACT=1
export NVSTUSB_FRAME_TIMER_TICK_BIAS=0
export MPV_DRM_STEREO_USB_PHASE_SCHED=1
export MPV_DRM_STEREO_USB_PHASE_US=0
export MPV_DRM_STEREO_USB_DRIFT_US_PER_S=0
```

The `r` base in mpv predicted-MSC mode remains 4629us. Each interval
starts at the **first actual USB SET_EYE call**, not from mpv startup;
all slots last exactly 20s as measured with CLOCK_MONOTONIC.
The five-slot sequence then repeats:

| Time since first command | r (us) | Phase |
| --- | ---: | --- |
| 0–20 s | 4629 | baseline |
| 20–40 s | 4129 | -500 |
| 40–60 s | 4629 | baseline |
| 60–80 s | 5129 | +500 |
| 80–100 s | 4629 | baseline |
| 100–120 s | 4629 | repeat baseline |
| 120–140 s | 4129 | repeat -500 |

Use the same `MPV_DRM_STEREO_USB_LIBRARY` and
`MPV_DRM_STEREO_USB_FIRMWARE` as prior tests and save library stderr
separately. Observe whether the **boundary moves abruptly at 20s,
40s, 60s and 80s** and whether it returns near its previous position.
Do not use *time until the 0% marker disappears* as the main endpoint.
Even a visually negative outcome is useful evidence.

A single setup-time diagnostic
`nvstusb: SET_EYE step A/B: enabled base_us=4629.000 step_us=500 slot_seconds=20 ...`
confirms the mode. On shutdown,
`nvstusb: SET_EYE step A/B final ... packets=... transitions=... final_slot=...`
confirms execution. There are no logs or file writes per USB command.

`NVSTUSB_EYE_TIMER_STEP_US` accepts integer 0..500 (default 0/off);
`NVSTUSB_EYE_TIMER_STEP_SECONDS` accepts integer 5..60 (default 20).
The pure functions are covered by `make check`.
A nonzero per-command ramp takes precedence: if both the ramp and
step mode are requested the step mode is rejected with a warning.
The sequence only changes SET_EYE command parameter r, not startup
firmware frame reload z, host deadlines or KMS scanout.

IMPORTANT: `Stereo USB scheduler final fixed_eye_timer_us=4629`
continues to show the BASE readback: it cannot describe per-command
step values. This experiment is **not** measured IR and should not
be used as a long-movie configuration.

## A/B experiment (NO host phase drift)

Keep mpv at the existing known DRM/native-stereo startup options.
Use calibration mode 1 or 2 (complementary white and black scanout).
Keep the display at 120Hz and VRR disabled, and do not touch the phase
arrow keys during a run.

```sh
unset NVSTUSB_EYE_TIMER_US
export NVSTUSB_FRAME_TIMER_EXACT=1
export NVSTUSB_FRAME_TIMER_TICK_BIAS=0
export NVSTUSB_EYE_TIMER_RAMP_LIMIT_US=500
export MPV_DRM_STEREO_USB=1
export MPV_DRM_STEREO_USB_PHASE_SCHED=1
export MPV_DRM_STEREO_USB_PHASE_US=0
export MPV_DRM_STEREO_USB_DRIFT_US_PER_S=0
export MPV_DRM_STEREO_USB_EVENT_TRACE=0
export MPV_DRM_STEREO_USB_WORKER_LOG=0
```

Run the same mpv command for each case, changing only one setting:

| Run | `NVSTUSB_EYE_TIMER_RAMP_US_PER_S` | Why |
| --- | ---: | --- |
| A | `0` | Fixed r baseline (previously observed ~2-3min drift) |
| B | `-9` | Ramp r earlier/smaller, then stop after ~55.6s |
| C | `+9` | Ramp r later/larger, then stop after ~55.6s |

A/B before C is preferable; reverse signs if desired, and preserve
identical initial conditions. A rate of -9us/s with a 500us limit
reduces the **command-timer value** from ~4629us to ~4129us by
55.6s, *not* the host send deadline. This is a diagnostic step,
not a long-term synchronization setting.

Record the scanout contamination boundary at ~0, 30, 60, 90 and 150s
and whether it moves up or down. An immediate change in optical
contamination with different r-ramp directions would be useful
evidence that the firmware command timer affects perceived shutter
timing. Lack of a change would also be informative. Do not infer a
precise optical phase without a photodiode/IR detector.

Send library stderr to a separate file. This is emitted from
`libnvstusb`, not mpv's `--log-file`:

```sh
# Append to mpv command:
2> /tmp/nvstusb-set-eye-ramp.stderr

# Inspect AFTER shutdown:
grep -E 'T2 frame reload:|SET_EYE (timer ramp|ramp final):' \
  /tmp/nvstusb-set-eye-ramp.stderr
```

Expected startup `SET_EYE timer ramp: enabled ... rate_us_per_s=-9.000`
and shutdown `SET_EYE ramp final: ... packets=... last_us=...
clamped=1` verify that the intended code path ran. In run A, no ramp
diagnostics are printed because the experiment is disabled.

## Interpretation and limits

- Positive and negative r ramps produce different optical behavior:
  the SET_EYE command's timer influences the actual IR/eye state.
  Confirm with repeated sign-controlled tests and, ideally, measured IR.
- Neither sign changes the optical drift: changing r does not
  demonstrably correct the long-term optical phase; investigate
  Timer 2 interrupt/reload and shutter receiver dynamics instead.
- The shift strongly changes ghosting or shutter visibility:
  r may primarily affect how long/when the glasses open rather
  than the long-term cadence. Do not leave it enabled for movies.
- A sharp change when the ramp hits its `LIMIT_US` clamp is a
  deliberately introduced diagnostic artifact, NOT a firmware bug.
- Host send timing, firmware-timer counts and real IR/light transitions
  are three *different clocks*. No single software trace proves
  physical IR sync; suitable optical capture remains the decisive test.

No firmware image is modified or reflashed by this experiment.
