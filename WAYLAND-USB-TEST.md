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
