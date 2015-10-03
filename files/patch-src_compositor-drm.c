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
@@ -89,11 +106,15 @@
 	struct weston_backend base;
 	struct weston_compositor *compositor;
 
+#if !defined(__FreeBSD__)
 	struct udev *udev;
+#endif
 	struct wl_event_source *drm_source;
 
+#if !defined(__FreeBSD__)
 	struct udev_monitor *udev_monitor;
 	struct wl_event_source *udev_drm_source;
+#endif
 
 	struct {
 		int id;
@@ -126,7 +147,9 @@
 
 	uint32_t prev_state;
 
+#if !defined(__FreeBSD__)
 	struct udev_input input;
+#endif
 
 	int32_t cursor_width;
 	int32_t cursor_height;
@@ -184,7 +207,9 @@
 	struct weston_view *cursor_view;
 	int current_cursor;
 	struct drm_fb *current, *next;
+#if !defined(__FreeBSD__)
 	struct backlight *backlight;
+#endif
 
 	struct drm_fb *dumb[2];
 	pixman_image_t *image[2];
@@ -391,7 +416,7 @@
 				    format, handles, pitches, offsets,
 				    &fb->fb_id, 0);
 		if (ret) {
-			weston_log("addfb2 failed: %m\n");
+			weston_log("addfb2 failed: %s\n", strerror(errno));
 			backend->no_addfb2 = 1;
 			backend->sprites_are_broken = 1;
 		}
@@ -402,7 +427,7 @@
 				   fb->stride, fb->handle, &fb->fb_id);
 
 	if (ret) {
-		weston_log("failed to create kms fb: %m\n");
+		weston_log("failed to create kms fb: %s\n", strerror(errno));
 		goto err_free;
 	}
 
@@ -532,7 +557,8 @@
 
 	bo = gbm_surface_lock_front_buffer(output->surface);
 	if (!bo) {
-		weston_log("failed to lock front buffer: %m\n");
+		weston_log("failed to lock front buffer: %s\n",
+		    strerror(errno));
 		return;
 	}
 
@@ -604,7 +630,7 @@
 				 output->crtc_id,
 				 size, r, g, b);
 	if (rc)
-		weston_log("set gamma failed: %m\n");
+		weston_log("set gamma failed: %s\n", strerror(errno));
 }
 
 /* Determine the type of vblank synchronization to use for the output.
@@ -659,7 +685,7 @@
 				     &output->connector_id, 1,
 				     &mode->mode_info);
 		if (ret) {
-			weston_log("set mode failed: %m\n");
+			weston_log("set mode failed: %s\n", strerror(errno));
 			goto err_pageflip;
 		}
 		output_base->set_dpms(output_base, WESTON_DPMS_ON);
@@ -668,7 +694,7 @@
 	if (drmModePageFlip(backend->drm.fd, output->crtc_id,
 			    output->next->fb_id,
 			    DRM_MODE_PAGE_FLIP_EVENT, output) < 0) {
-		weston_log("queueing pageflip failed: %m\n");
+		weston_log("queueing pageflip failed: %s\n", strerror(errno));
 		goto err_pageflip;
 	}
 
@@ -790,7 +816,7 @@
 
 	if (drmModePageFlip(backend->drm.fd, output->crtc_id, fb_id,
 			    DRM_MODE_PAGE_FLIP_EVENT, output) < 0) {
-		weston_log("queueing pageflip failed: %m\n");
+		weston_log("queueing pageflip failed: %s\n", strerror(errno));
 		goto finish_frame;
 	}
 
@@ -1154,7 +1180,7 @@
 	wl_shm_buffer_end_access(buffer->shm_buffer);
 
 	if (gbm_bo_write(bo, buf, sizeof buf) < 0)
-		weston_log("failed update cursor: %m\n");
+		weston_log("failed update cursor: %s\n", strerror(errno));
 }
 
 static void
@@ -1187,7 +1213,8 @@
 		handle = gbm_bo_get_handle(bo).s32;
 		if (drmModeSetCursor(b->drm.fd, output->crtc_id, handle,
 				b->cursor_width, b->cursor_height)) {
-			weston_log("failed to set cursor: %m\n");
+			weston_log("failed to set cursor: %s\n",
+			    strerror(errno));
 			b->cursors_are_broken = 1;
 		}
 	}
@@ -1196,7 +1223,8 @@
 	y = (ev->geometry.y - output->base.y) * output->base.current_scale;
 	if (output->cursor_plane.x != x || output->cursor_plane.y != y) {
 		if (drmModeMoveCursor(b->drm.fd, output->crtc_id, x, y)) {
-			weston_log("failed to move cursor: %m\n");
+			weston_log("failed to move cursor: %s\n",
+			    strerror(errno));
 			b->cursors_are_broken = 1;
 		}
 
@@ -1305,8 +1333,10 @@
 		return;
 	}
 
+#if !defined(__FreeBSD__)
 	if (output->backlight)
 		backlight_destroy(output->backlight);
+#endif
 
 	drmModeFreeProperty(output->dpms_prop);
 
@@ -1454,13 +1484,19 @@
 }
 
 static int
+#if defined(__FreeBSD__)
+init_drm(struct drm_backend *b, const char *filename)
+#else
 init_drm(struct drm_backend *b, struct udev_device *device)
+#endif
 {
-	const char *filename, *sysnum;
 	uint64_t cap;
 	int fd, ret;
 	clockid_t clk_id;
 
+#if !defined(__FreeBSD__)
+	const char *filename, *sysnum;
+
 	sysnum = udev_device_get_sysnum(device);
 	if (sysnum)
 		b->drm.id = atoi(sysnum);
@@ -1470,11 +1506,16 @@
 	}
 
 	filename = udev_device_get_devnode(device);
+#endif
 	fd = weston_launcher_open(b->compositor->launcher, filename, O_RDWR);
 	if (fd < 0) {
 		/* Probably permissions error */
+#if defined(__FreeBSD__)
+		weston_log("couldn't open %s, skipping\n", filename);
+#else
 		weston_log("couldn't open %s, skipping\n",
 			udev_device_get_devnode(device));
+#endif
 		return -1;
 	}
 
@@ -1490,8 +1531,13 @@
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
 
@@ -1666,6 +1712,7 @@
 	}
 }
 
+#if !defined(__FreeBSD__)
 /* returns a value between 0-255 range, where higher is brighter */
 static uint32_t
 drm_get_backlight(struct drm_output *output)
@@ -1701,6 +1748,7 @@
 
 	backlight_set_brightness(output->backlight, new_brightness);
 }
+#endif
 
 static drmModePropertyPtr
 drm_get_prop(int fd, drmModeConnectorPtr connector, const char *name)
@@ -2105,6 +2153,7 @@
 	return 0;
 }
 
+#if !defined(__FreeBSD__)
 static void
 setup_output_seat_constraint(struct drm_backend *b,
 			     struct weston_output *output,
@@ -2127,6 +2176,7 @@
 					     &pointer->y);
 	}
 }
+#endif
 
 static int
 get_gbm_format_from_section(struct weston_config_section *section,
@@ -2276,7 +2326,11 @@
 create_output_for_connector(struct drm_backend *b,
 			    drmModeRes *resources,
 			    drmModeConnector *connector,
+#if defined(__FreeBSD__)
+			    int x, int y)
+#else
 			    int x, int y, struct udev_device *drm_device)
+#endif
 {
 	struct drm_output *output;
 	struct drm_mode *drm_mode, *next, *current;
@@ -2338,8 +2392,10 @@
 					&output->format) == -1)
 		output->format = b->format;
 
+#if !defined(__FreeBSD__)
 	weston_config_section_get_string(section, "seat", &s, "");
 	setup_output_seat_constraint(b, &output->base, s);
+#endif
 	free(s);
 
 	output->crtc_id = resources->crtcs[i];
@@ -2389,6 +2445,7 @@
 		goto err_output;
 	}
 
+#if !defined(__FreeBSD__)
 	output->backlight = backlight_init(drm_device,
 					   connector->connector_type);
 	if (output->backlight) {
@@ -2399,6 +2456,7 @@
 	} else {
 		weston_log("Failed to initialize backlight\n");
 	}
+#endif
 
 	weston_compositor_add_output(b->compositor, &output->base);
 
@@ -2528,8 +2586,12 @@
 }
 
 static int
+#if defined(__FreeBSD__)
+create_outputs(struct drm_backend *b, uint32_t option_connector)
+#else
 create_outputs(struct drm_backend *b, uint32_t option_connector,
 	       struct udev_device *drm_device)
+#endif
 {
 	drmModeConnector *connector;
 	drmModeRes *resources;
@@ -2566,8 +2628,12 @@
 		    (option_connector == 0 ||
 		     connector->connector_id == option_connector)) {
 			if (create_output_for_connector(b, resources,
+#if defined(__FreeBSD__)
+							connector, x, y) < 0) {
+#else
 							connector, x, y,
 							drm_device) < 0) {
+#endif
 				drmModeFreeConnector(connector);
 				continue;
 			}
@@ -2591,6 +2657,7 @@
 	return 0;
 }
 
+#if !defined(__FreeBSD__)
 static void
 update_outputs(struct drm_backend *b, struct udev_device *drm_device)
 {
@@ -2693,6 +2760,7 @@
 
 	return 1;
 }
+#endif
 
 static void
 drm_restore(struct weston_compositor *ec)
@@ -2705,9 +2773,13 @@
 {
 	struct drm_backend *b = (struct drm_backend *) ec->backend;
 
+#if defined(__FreeBSD__)
+	/* XXX destroy keyboard/mouse input sources */
+#else
 	udev_input_destroy(&b->input);
 
 	wl_event_source_remove(b->udev_drm_source);
+#endif
 	wl_event_source_remove(b->drm_source);
 
 	destroy_sprites(b);
@@ -2749,9 +2821,10 @@
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
@@ -2769,10 +2842,18 @@
 		compositor->state = b->prev_state;
 		drm_backend_set_modes(b);
 		weston_compositor_damage_all(compositor);
+#if defined(__FreeBSD__)
+		/* XXX enable keyboard/mouse input sources */
+#else
 		udev_input_enable(&b->input);
+#endif
 	} else {
 		weston_log("deactivating session\n");
+#if defined(__FreeBSD__)
+		/* XXX disable keyboard/mouse input sources */
+#else
 		udev_input_disable(&b->input);
+#endif
 
 		b->prev_state = compositor->state;
 		weston_compositor_offscreen(compositor);
@@ -2810,6 +2891,7 @@
 	weston_launcher_activate_vt(compositor->launcher, key - KEY_F1 + 1);
 }
 
+#if !defined(__FreeBSD__)
 /*
  * Find primary GPU
  * Some systems may have multiple DRM devices attached to a single seat. This
@@ -2865,6 +2947,7 @@
 	udev_enumerate_unref(e);
 	return drm_device;
 }
+#endif
 
 static void
 planes_binding(struct weston_keyboard *keyboard, uint32_t time, uint32_t key,
@@ -2925,7 +3008,7 @@
 	ret = vaapi_recorder_frame(output->recorder, fd,
 				   output->current->stride);
 	if (ret < 0) {
-		weston_log("[libva recorder] aborted: %m\n");
+		weston_log("[libva recorder] aborted: %s\n", strerror(errno));
 		recorder_destroy(output);
 	}
 }
@@ -3059,7 +3142,9 @@
 {
 	struct drm_backend *b;
 	struct weston_config_section *section;
+#if !defined(__FreeBSD__)
 	struct udev_device *drm_device;
+#endif
 	struct wl_event_loop *loop;
 	const char *path;
 	uint32_t key;
@@ -3100,23 +3185,33 @@
 		goto err_compositor;
 	}
 
+#if !defined(__FreeBSD__)
 	b->udev = udev_new();
 	if (b->udev == NULL) {
 		weston_log("failed to initialize udev context\n");
 		goto err_launcher;
 	}
+#endif
 
 	b->session_listener.notify = session_notify;
 	wl_signal_add(&compositor->session_signal, &b->session_listener);
 
+#if defined(__FreeBSD__)
+	path = "/dev/dri/card0";
+#else
 	drm_device = find_primary_gpu(b, param->seat_id);
 	if (drm_device == NULL) {
 		weston_log("no drm device found\n");
 		goto err_udev;
 	}
 	path = udev_device_get_syspath(drm_device);
+#endif
 
+#if defined(__FreeBSD__)
+	if (init_drm(b, path) < 0) {
+#else
 	if (init_drm(b, drm_device) < 0) {
+#endif
 		weston_log("failed to initialize kms\n");
 		goto err_udev_dev;
 	}
@@ -3146,13 +3241,21 @@
 	wl_list_init(&b->sprite_list);
 	create_sprites(b);
 
+#if defined(__FreeBSD__)
+	/* XXX */
+#else
 	if (udev_input_init(&b->input,
 			    compositor, b->udev, param->seat_id) < 0) {
 		weston_log("failed to create input devices\n");
 		goto err_sprite;
 	}
+#endif
 
+#if defined(__FreeBSD__)
+	if (create_outputs(b, param->connector) < 0) {
+#else
 	if (create_outputs(b, param->connector, drm_device) < 0) {
+#endif
 		weston_log("failed to create output for %s\n", path);
 		goto err_udev_input;
 	}
@@ -3169,6 +3272,7 @@
 		wl_event_loop_add_fd(loop, b->drm.fd,
 				     WL_EVENT_READABLE, on_drm_input, b);
 
+#if !defined(__FreeBSD__)
 	b->udev_monitor = udev_monitor_new_from_netlink(b->udev, "udev");
 	if (b->udev_monitor == NULL) {
 		weston_log("failed to intialize udev monitor\n");
@@ -3187,6 +3291,7 @@
 	}
 
 	udev_device_unref(drm_device);
+#endif
 
 	weston_compositor_add_debug_binding(compositor, KEY_O,
 					    planes_binding, b);
@@ -3209,22 +3314,30 @@
 
 	return b;
 
+#if !defined(__FreeBSD__)
 err_udev_monitor:
 	wl_event_source_remove(b->udev_drm_source);
 	udev_monitor_unref(b->udev_monitor);
 err_drm_source:
+#endif
 	wl_event_source_remove(b->drm_source);
 err_udev_input:
+#if !defined(__FreeBSD__)
 	udev_input_destroy(&b->input);
 err_sprite:
+#endif
 	gbm_device_destroy(b->gbm);
 	destroy_sprites(b);
 err_udev_dev:
+#if !defined(__FreeBSD__)
 	udev_device_unref(drm_device);
 err_launcher:
+#endif
 	weston_launcher_destroy(compositor->launcher);
+#if !defined(__FreeBSD__)
 err_udev:
 	udev_unref(b->udev);
+#endif
 err_compositor:
 	weston_compositor_shutdown(compositor);
 err_base:
@@ -3241,7 +3354,9 @@
 
 	const struct weston_option drm_options[] = {
 		{ WESTON_OPTION_INTEGER, "connector", 0, &param.connector },
+#if !defined(__FreeBSD__)
 		{ WESTON_OPTION_STRING, "seat", 0, &param.seat_id },
+#endif
 		{ WESTON_OPTION_INTEGER, "tty", 0, &param.tty },
 		{ WESTON_OPTION_BOOLEAN, "current-mode", 0, &option_current_mode },
 		{ WESTON_OPTION_BOOLEAN, "use-pixman", 0, &param.use_pixman },
