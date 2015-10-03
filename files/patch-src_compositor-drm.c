--- src/compositor-drm.c.orig	2015-10-03 13:24:40 +0200
+++ src/compositor-drm.c
@@ -33,7 +33,14 @@
 #include <fcntl.h>
 #include <unistd.h>
 #include <linux/input.h>
+#if defined(__FreeBSD__)
+#include <sys/consio.h>
+#include <sys/mouse.h>
+#include <sys/ioctl.h>
+#include <sys/socket.h>
+#else
 #include <linux/vt.h>
+#endif
 #include <assert.h>
 #include <sys/mman.h>
 #include <dlfcn.h>
@@ -44,19 +51,32 @@
 #include <drm_fourcc.h>
 
 #include <gbm.h>
+#if defined(__FreeBSD__)
+//#include <EGL/egl.h>
+#include <kbdev.h>
+#else
 #include <libudev.h>
+#endif
 
 #include "shared/helpers.h"
 #include "shared/timespec-util.h"
+#if !defined(__FreeBSD__)
 #include "libbacklight.h"
+#endif
 #include "compositor.h"
+#if !defined(__FreeBSD__)
 #include "gl-renderer.h"
+#endif
 #include "pixman-renderer.h"
+#if !defined(__FreeBSD__)
 #include "libinput-seat.h"
 #include "launcher-util.h"
+#endif
 #include "vaapi-recorder.h"
 #include "presentation_timing-server-protocol.h"
+#if !defined(__FreeBSD__)
 #include "linux-dmabuf.h"
+#endif
 
 #ifndef DRM_CAP_TIMESTAMP_MONOTONIC
 #define DRM_CAP_TIMESTAMP_MONOTONIC 0x6
