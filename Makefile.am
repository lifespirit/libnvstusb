SUBDIRS = src tools

# Build the USB-only shutter diagnostic without manual compiler flags.
noinst_PROGRAMS = test-usb test-usb-wave
check_PROGRAMS = test-frame-timer
TESTS = test-frame-timer

test_frame_timer_SOURCES = tests/test-frame-timer.c
test_frame_timer_CPPFLAGS = -I$(srcdir)/include
test_frame_timer_LDADD = -lm
test_usb_SOURCES = test-usb.c
test_usb_CPPFLAGS = -I$(srcdir)/include
test_usb_LDADD = src/libnvstusb.la -lm

# Second USB-only diagnostic: sinusoidal 120 -> 115 -> 120 Hz
# command cadence. Firmware is initialized at 120 Hz just once.
test_usb_wave_SOURCES = test-usb-wave.c
test_usb_wave_CPPFLAGS = -I$(srcdir)/include
test_usb_wave_LDADD = src/libnvstusb.la -lm

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = pkgconfig/libnvstusb.pc
