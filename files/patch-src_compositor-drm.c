--- src/compositor-drm.c.orig	2015-09-14 20:23:31 +0200
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
@@ -44,19 +51,29 @@
 #include <drm_fourcc.h>
 
 #include <gbm.h>
+#if defined(__FreeBSD__)
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
 #include "gl-renderer.h"
 #include "pixman-renderer.h"
+#if !defined(__FreeBSD__)
 #include "libinput-seat.h"
+#endif
 #include "launcher-util.h"
 #include "vaapi-recorder.h"
 #include "presentation_timing-server-protocol.h"
+//#if !defined(__FreeBSD__)
 #include "linux-dmabuf.h"
+//#endif
 
 #ifndef DRM_CAP_TIMESTAMP_MONOTONIC
 #define DRM_CAP_TIMESTAMP_MONOTONIC 0x6
@@ -126,7 +143,9 @@
 
 	uint32_t prev_state;
 
+#if !defined(__FreeBSD__)
 	struct udev_input input;
+#endif
 
 	int32_t cursor_width;
 	int32_t cursor_height;
@@ -184,7 +203,9 @@
 	struct weston_view *cursor_view;
 	int current_cursor;
 	struct drm_fb *current, *next;
+#if !defined(__FreeBSD__)
 	struct backlight *backlight;
+#endif
 
 	struct drm_fb *dumb[2];
 	pixman_image_t *image[2];
@@ -391,7 +412,7 @@
 				    format, handles, pitches, offsets,
 				    &fb->fb_id, 0);
 		if (ret) {
-			weston_log("addfb2 failed: %m\n");
+			weston_log("addfb2 failed: %s\n", strerror(errno));
 			backend->no_addfb2 = 1;
 			backend->sprites_are_broken = 1;
 		}
@@ -402,7 +423,7 @@
 				   fb->stride, fb->handle, &fb->fb_id);
 
 	if (ret) {
-		weston_log("failed to create kms fb: %m\n");
+		weston_log("failed to create kms fb: %s\n", strerror(errno));
 		goto err_free;
 	}
 
@@ -532,7 +553,8 @@
 
 	bo = gbm_surface_lock_front_buffer(output->surface);
 	if (!bo) {
-		weston_log("failed to lock front buffer: %m\n");
+		weston_log("failed to lock front buffer: %s\n",
+		    strerror(errno));
 		return;
 	}
 
@@ -604,7 +626,7 @@
 				 output->crtc_id,
 				 size, r, g, b);
 	if (rc)
-		weston_log("set gamma failed: %m\n");
+		weston_log("set gamma failed: %s\n", strerror(errno));
 }
 
 /* Determine the type of vblank synchronization to use for the output.
@@ -659,7 +681,7 @@
 				     &output->connector_id, 1,
 				     &mode->mode_info);
 		if (ret) {
-			weston_log("set mode failed: %m\n");
+			weston_log("set mode failed: %s\n", strerror(errno));
 			goto err_pageflip;
 		}
 		output_base->set_dpms(output_base, WESTON_DPMS_ON);
@@ -668,7 +690,7 @@
 	if (drmModePageFlip(backend->drm.fd, output->crtc_id,
 			    output->next->fb_id,
 			    DRM_MODE_PAGE_FLIP_EVENT, output) < 0) {
-		weston_log("queueing pageflip failed: %m\n");
+		weston_log("queueing pageflip failed: %s\n", strerror(errno));
 		goto err_pageflip;
 	}
 
@@ -790,7 +812,7 @@
 
 	if (drmModePageFlip(backend->drm.fd, output->crtc_id, fb_id,
 			    DRM_MODE_PAGE_FLIP_EVENT, output) < 0) {
-		weston_log("queueing pageflip failed: %m\n");
+		weston_log("queueing pageflip failed: %s\n", strerror(errno));
 		goto finish_frame;
 	}
 
@@ -1154,7 +1176,7 @@
 	wl_shm_buffer_end_access(buffer->shm_buffer);
 
 	if (gbm_bo_write(bo, buf, sizeof buf) < 0)
-		weston_log("failed update cursor: %m\n");
+		weston_log("failed update cursor: %s\n", strerror(errno));
 }
 
 static void
@@ -1187,7 +1209,8 @@
 		handle = gbm_bo_get_handle(bo).s32;
 		if (drmModeSetCursor(b->drm.fd, output->crtc_id, handle,
 				b->cursor_width, b->cursor_height)) {
-			weston_log("failed to set cursor: %m\n");
+			weston_log("failed to set cursor: %s\n",
+			    strerror(errno));
 			b->cursors_are_broken = 1;
 		}
 	}
@@ -1196,7 +1219,8 @@
 	y = (ev->geometry.y - output->base.y) * output->base.current_scale;
 	if (output->cursor_plane.x != x || output->cursor_plane.y != y) {
 		if (drmModeMoveCursor(b->drm.fd, output->crtc_id, x, y)) {
-			weston_log("failed to move cursor: %m\n");
+			weston_log("failed to move cursor: %s\n",
+			    strerror(errno));
 			b->cursors_are_broken = 1;
 		}
 
@@ -1305,8 +1329,10 @@
 		return;
 	}
 
+#if !defined(__FreeBSD__)
 	if (output->backlight)
 		backlight_destroy(output->backlight);
+#endif
 
 	drmModeFreeProperty(output->dpms_prop);
 
@@ -1490,8 +1516,13 @@
 		clk_id = CLOCK_REALTIME;
 
 	if (weston_compositor_set_presentation_clock(b->compositor, clk_id) < 0) {
+#if defined(__FreeBSD__)
+		weston_log("Error: failed to set presentation clock %ld.\n",
+			   clk_id);
+#else
 		weston_log("Error: failed to set presentation clock %d.\n",
 			   clk_id);
+#endif
 		return -1;
 	}
 
@@ -1666,6 +1697,7 @@
 	}
 }
 
+#if !defined(__FreeBSD__)
 /* returns a value between 0-255 range, where higher is brighter */
 static uint32_t
 drm_get_backlight(struct drm_output *output)
@@ -1701,6 +1733,7 @@
 
 	backlight_set_brightness(output->backlight, new_brightness);
 }
+#endif
 
 static drmModePropertyPtr
 drm_get_prop(int fd, drmModeConnectorPtr connector, const char *name)
@@ -2389,6 +2422,7 @@
 		goto err_output;
 	}
 
+#if !defined(__FreeBSD__)
 	output->backlight = backlight_init(drm_device,
 					   connector->connector_type);
 	if (output->backlight) {
@@ -2399,6 +2433,7 @@
 	} else {
 		weston_log("Failed to initialize backlight\n");
 	}
+#endif
 
 	weston_compositor_add_output(b->compositor, &output->base);
 
@@ -2749,9 +2784,10 @@
 				     &drm_mode->mode_info);
 		if (ret < 0) {
 			weston_log(
-				"failed to set mode %dx%d for output at %d,%d: %m\n",
+				"failed to set mode %dx%d for output at %d,%d: %s\n",
 				drm_mode->base.width, drm_mode->base.height,
-				output->base.x, output->base.y);
+				output->base.x, output->base.y,
+				strerror(errno));
 		}
 	}
 }
@@ -2925,7 +2961,7 @@
 	ret = vaapi_recorder_frame(output->recorder, fd,
 				   output->current->stride);
 	if (ret < 0) {
-		weston_log("[libva recorder] aborted: %m\n");
+		weston_log("[libva recorder] aborted: %s\n", strerror(errno));
 		recorder_destroy(output);
 	}
 }
