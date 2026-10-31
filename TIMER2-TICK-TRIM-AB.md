# Timer 2 one-tick trim experiment

Branch: `feature/timer2-tick-trim-diagnostics` (derived from
`feature/timer2-exact-period-diagnostics`). This experiment changes
**only** the startup firmware T2 frame reload at address 0x201b. It
does not change the per-eye SET_EYE command timer, KMS scheduling, or
IR active interval.

## Why this test

Optically reported drift with `NVSTUSB_FRAME_TIMER_EXACT=1` was roughly
20% of the screen height over 180–240 seconds. Assuming a near-linear
raster scan at 120Hz, this is 1.67ms accumulated phase, or about
7–9 microseconds/second (7–9ppm). It also agrees in order of magnitude
with manual command-phase corrections seen on the earlier full-movie run.

At nominal 12MHz T2, a single tick is 83.333ns. One tick per 120Hz
frame corresponds to approximately 10us/s (10ppm) rate change, enough
to test whether the frame reload affects the measured optical drift.

**This is NOT proof of a free-running timer.** Firmware command semantics,
IR emission, glasses/receiver clock and actual quartz accuracy are unknown.
We are changing a register and observing *whether* drift responds.

## Build

```sh
cd ~/Projects/src/libnvstusb
git fetch origin feature/timer2-tick-trim-diagnostics
git switch --track origin/feature/timer2-tick-trim-diagnostics
autoreconf -fi
./configure
make -j"$(nproc)"
make check
```

The pre-existing mpv experimental branch can stay unchanged:
`feature/drm-stereo-usb-phase-scheduler` at commit `4473649`.
Point mpv's `MPV_DRM_STEREO_USB_LIBRARY` to the freshly built library.

## Compare the three settings, independently

Always use `NVSTUSB_FRAME_TIMER_EXACT=1`, with one of:

| Test | `NVSTUSB_FRAME_TIMER_TICK_BIAS` | Meaning |
|---|---|---|
| A | `0` | Exact-period baseline |
| B | `-1` | One shorter hardware tick per nominal frame |
| C | `+1` | One longer hardware tick per nominal frame |

The supported diagnostic range is -2..+2 ticks. A trim without
`NVSTUSB_FRAME_TIMER_EXACT=1` is deliberately ignored to avoid
changing the legacy behavior. Default is zero.

Use the identical mpv command, firmware, physical USB port, display
mode and `MPV_DRM_STEREO_USB_PHASE_US=-1600` for all runs. Do not set
`NVSTUSB_EYE_TIMER_US`. Keep the same synthetic L/R calibration
pattern and do **not** adjust the arrow-key phase during each sample.
Observe the moving white/black boundary for 3–5 minutes at 0, -1
and +1; repeat in shuffled order if the results differ.

Record starting boundary (percent of screen height), boundary at 60s,
120s and 180s, and **which direction** it moves. Initial position
between runs may be random; the slope is what matters. Prefer a
camera/optical sensor over prolonged unstable viewing through glasses.

Capture the library's stderr separately, because `--log-file` is
mpv's logger and may not include the library's diagnostic:
`2> /tmp/nvstusb-trim-minus1.stderr` while preserving stdout as a
TTY for mpv's terminal keys.

The new line:
```text
nvstusb: T2 frame reload: mode=exact ... tick_bias=-1
selected_ticks=... selected=...us selected_model_drift=...us/s
(nominal 12MHz, not measured IR)
```
confirms the active hardware-register calculation.

At exactly 119.998Hz, the **nominal model only** predicts ~+3.3us/s
for bias=0, ~-6.7us/s for bias=-1 and ~+13.3us/s for bias=+1.
This does not include device crystal error, display refresh
uncertainty, IR delay or receiver phase behavior.

## Interpret outcomes

- Distinct drift rates, consistently separated by ~10us/s across one
  tick, strongly support influence of T2 frame-reload timing.
  A constant bias may then reduce drift. If 1 tick is too coarse,
  fractional compensation would require a separately justified
  mechanism (e.g. runtime firmware support or fractional rescheduling).
- All three runs drifting at approximately the same rate / direction
  suggests we need to inspect SET_EYE command semantics, transmitter
  firmware/receiver timing, or actual IR edges.
- Abrupt phase jumps indicate a different failure mode from gradual
  ppm drift; collect time-correlated host USB data and ideally
  physical IR+shutter measurements. This static trim should not be
  expected to prevent a sudden lost eye phase.

Do not run long movies with uncomfortable or unstable stereoscopy.
