lib_LTLIBRARIES = libnvstusb.la
libnvstusbdir=$(includedir)/libnvstusb
libnvstusb_la_SOURCES = nvstusb.c usb_libusb.c
libnvstusb_la_CPPFLAGS = -I@top_srcdir@/include ${LIBUSB_CFLAGS}
libnvstusb_la_LIBADD = ${LIBUSB_LIBS} ${GL_LIBS} ${X11_LIBS} -lm
libnvstusb_la_LDFLAGS = -no-undefined
libnvstusb_HEADERS = @top_srcdir@/include/usb.h @top_srcdir@/include/nvstusb.h
