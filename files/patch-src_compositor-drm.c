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
@@ -89,11 +106,24 @@
 	struct weston_backend base;
 	struct weston_compositor *compositor;
 
+#if defined(__FreeBSD__)
+	struct weston_seat syscons_seat;
+
+	int sysmouse_fd;
+	struct wl_event_source *sysmouse_source;
+
+	int kbd_fd;
+	struct kbdev_state *kbdst;
+	struct wl_event_source *kbd_source;
+#else
 	struct udev *udev;
+#endif
 	struct wl_event_source *drm_source;
 
+#if !defined(__FreeBSD__)
 	struct udev_monitor *udev_monitor;
 	struct wl_event_source *udev_drm_source;
+#endif
 
 	struct {
 		int id;
@@ -126,7 +156,9 @@
 
 	uint32_t prev_state;
 
+#if !defined(__FreeBSD__)
 	struct udev_input input;
+#endif
 
 	int32_t cursor_width;
 	int32_t cursor_height;
@@ -184,7 +216,9 @@
 	struct weston_view *cursor_view;
 	int current_cursor;
 	struct drm_fb *current, *next;
+#if !defined(__FreeBSD__)
 	struct backlight *backlight;
+#endif
 
 	struct drm_fb *dumb[2];
 	pixman_image_t *image[2];
@@ -391,7 +425,7 @@
 				    format, handles, pitches, offsets,
 				    &fb->fb_id, 0);
 		if (ret) {
-			weston_log("addfb2 failed: %m\n");
+			weston_log("addfb2 failed: %s\n", strerror(errno));
 			backend->no_addfb2 = 1;
 			backend->sprites_are_broken = 1;
 		}
@@ -402,7 +436,7 @@
 				   fb->stride, fb->handle, &fb->fb_id);
 
 	if (ret) {
-		weston_log("failed to create kms fb: %m\n");
+		weston_log("failed to create kms fb: %s\n", strerror(errno));
 		goto err_free;
 	}
 
@@ -532,7 +566,8 @@
 
 	bo = gbm_surface_lock_front_buffer(output->surface);
 	if (!bo) {
-		weston_log("failed to lock front buffer: %m\n");
+		weston_log("failed to lock front buffer: %s\n",
+		    strerror(errno));
 		return;
 	}
 
@@ -604,7 +639,7 @@
 				 output->crtc_id,
 				 size, r, g, b);
 	if (rc)
-		weston_log("set gamma failed: %m\n");
+		weston_log("set gamma failed: %s\n", strerror(errno));
 }
 
 /* Determine the type of vblank synchronization to use for the output.
@@ -659,7 +694,7 @@
 				     &output->connector_id, 1,
 				     &mode->mode_info);
 		if (ret) {
-			weston_log("set mode failed: %m\n");
+			weston_log("set mode failed: %s\n", strerror(errno));
 			goto err_pageflip;
 		}
 		output_base->set_dpms(output_base, WESTON_DPMS_ON);
@@ -668,7 +703,7 @@
 	if (drmModePageFlip(backend->drm.fd, output->crtc_id,
 			    output->next->fb_id,
 			    DRM_MODE_PAGE_FLIP_EVENT, output) < 0) {
-		weston_log("queueing pageflip failed: %m\n");
+		weston_log("queueing pageflip failed: %s\n", strerror(errno));
 		goto err_pageflip;
 	}
 
@@ -790,7 +825,7 @@
 
 	if (drmModePageFlip(backend->drm.fd, output->crtc_id, fb_id,
 			    DRM_MODE_PAGE_FLIP_EVENT, output) < 0) {
-		weston_log("queueing pageflip failed: %m\n");
+		weston_log("queueing pageflip failed: %s\n", strerror(errno));
 		goto finish_frame;
 	}
 
@@ -1154,7 +1189,7 @@
 	wl_shm_buffer_end_access(buffer->shm_buffer);
 
 	if (gbm_bo_write(bo, buf, sizeof buf) < 0)
-		weston_log("failed update cursor: %m\n");
+		weston_log("failed update cursor: %s\n", strerror(errno));
 }
 
 static void
@@ -1187,7 +1222,8 @@
 		handle = gbm_bo_get_handle(bo).s32;
 		if (drmModeSetCursor(b->drm.fd, output->crtc_id, handle,
 				b->cursor_width, b->cursor_height)) {
-			weston_log("failed to set cursor: %m\n");
+			weston_log("failed to set cursor: %s\n",
+			    strerror(errno));
 			b->cursors_are_broken = 1;
 		}
 	}
@@ -1196,7 +1232,8 @@
 	y = (ev->geometry.y - output->base.y) * output->base.current_scale;
 	if (output->cursor_plane.x != x || output->cursor_plane.y != y) {
 		if (drmModeMoveCursor(b->drm.fd, output->crtc_id, x, y)) {
-			weston_log("failed to move cursor: %m\n");
+			weston_log("failed to move cursor: %s\n",
+			    strerror(errno));
 			b->cursors_are_broken = 1;
 		}
 
@@ -1305,8 +1342,10 @@
 		return;
 	}
 
+#if !defined(__FreeBSD__)
 	if (output->backlight)
 		backlight_destroy(output->backlight);
+#endif
 
 	drmModeFreeProperty(output->dpms_prop);
 
@@ -1454,13 +1493,19 @@
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
@@ -1470,11 +1515,16 @@
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
 
@@ -1490,8 +1540,13 @@
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
 
@@ -1666,6 +1721,7 @@
 	}
 }
 
+#if !defined(__FreeBSD__)
 /* returns a value between 0-255 range, where higher is brighter */
 static uint32_t
 drm_get_backlight(struct drm_output *output)
@@ -1701,6 +1757,7 @@
 
 	backlight_set_brightness(output->backlight, new_brightness);
 }
+#endif
 
 static drmModePropertyPtr
 drm_get_prop(int fd, drmModeConnectorPtr connector, const char *name)
@@ -2105,6 +2162,7 @@
 	return 0;
 }
 
+#if !defined(__FreeBSD__)
 static void
 setup_output_seat_constraint(struct drm_backend *b,
 			     struct weston_output *output,
@@ -2127,6 +2185,7 @@
 					     &pointer->y);
 	}
 }
+#endif
 
 static int
 get_gbm_format_from_section(struct weston_config_section *section,
@@ -2276,7 +2335,11 @@
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
@@ -2338,8 +2401,10 @@
 					&output->format) == -1)
 		output->format = b->format;
 
+#if !defined(__FreeBSD__)
 	weston_config_section_get_string(section, "seat", &s, "");
 	setup_output_seat_constraint(b, &output->base, s);
+#endif
 	free(s);
 
 	output->crtc_id = resources->crtcs[i];
@@ -2389,6 +2454,7 @@
 		goto err_output;
 	}
 
+#if !defined(__FreeBSD__)
 	output->backlight = backlight_init(drm_device,
 					   connector->connector_type);
 	if (output->backlight) {
@@ -2399,6 +2465,7 @@
 	} else {
 		weston_log("Failed to initialize backlight\n");
 	}
+#endif
 
 	weston_compositor_add_output(b->compositor, &output->base);
 
@@ -2528,8 +2595,12 @@
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
@@ -2566,8 +2637,12 @@
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
@@ -2591,6 +2666,7 @@
 	return 0;
 }
 
+#if !defined(__FreeBSD__)
 static void
 update_outputs(struct drm_backend *b, struct udev_device *drm_device)
 {
@@ -2693,6 +2769,149 @@
 
 	return 1;
 }
+#endif
+
+#if defined(__FreeBSD__)
+static int
+drm_kbd_handler(int fd, uint32_t mask, void *data)
+{
+	int i, n;
+	struct kbdev_event evs[64];
+
+	struct drm_backend *b = (struct drm_backend *)data;
+
+	fcntl(b->kbd_fd, F_SETFL, O_NONBLOCK);
+	n = kbdev_read_events(b->kbdst, evs, 64);
+	if (n <= 0)
+		return 0;
+
+	for (i = 0; i < n; i++) {
+		notify_key(&b->syscons_seat, weston_compositor_get_time(),
+		    evs[i].keycode,
+		    evs[i].pressed ? WL_KEYBOARD_KEY_STATE_PRESSED
+				   : WL_KEYBOARD_KEY_STATE_RELEASED,
+		    STATE_UPDATE_AUTOMATIC);
+	}
+
+	return 1;
+}
+
+static int
+drm_sysmouse_handler(int fd, uint32_t mask, void *data)
+{
+	char buf[128];
+	int xdelta, ydelta, zdelta;
+	wl_fixed_t xf, yf;
+	int nm;
+	static int oldmask = 7;
+	uint32_t time;
+	int k, n;
+
+	struct drm_backend *b = (struct drm_backend *)data;
+
+	k = 0;
+	if ((n = read(b->sysmouse_fd, buf, sizeof(buf))) <= 0)
+		return 0;
+
+	for (k = 0; k < n; k += 8) {
+		if (n -k < 8 || (buf[0] & 0x80) == 0 || (buf[7] & 0x80) != 0)
+			continue;
+
+		xdelta = buf[k+1] + buf[k+3];
+		ydelta = buf[k+2] + buf[k+4];
+		ydelta = -ydelta;
+		zdelta = (buf[k+5] > 0 && buf[k+6] == 0) ? buf[k+5] | 0x80 : buf[k+5] + buf[k+6];
+
+		time = weston_compositor_get_time();
+
+		notify_axis(&b->syscons_seat, weston_compositor_get_time(),
+		    WL_POINTER_AXIS_VERTICAL_SCROLL,
+		    wl_fixed_from_int(((char)zdelta)*10));
+
+		if (xdelta != 0 || ydelta != 0) {
+			xf = wl_fixed_from_int(xdelta);
+			yf = wl_fixed_from_int(ydelta);
+			notify_motion(&b->syscons_seat, time, xf, yf);
+		}
+
+		nm = buf[k+0] & 7;
+		if (nm != oldmask) {
+			if ((nm & 4) != (oldmask & 4)) {
+				notify_button(&b->syscons_seat, time, BTN_LEFT,
+				    (nm & 4) ? WL_POINTER_BUTTON_STATE_RELEASED
+					     : WL_POINTER_BUTTON_STATE_PRESSED);
+			}
+			if ((nm & 2) != (oldmask & 2)) {
+				notify_button(&b->syscons_seat, time, BTN_MIDDLE,
+				    (nm & 2) ? WL_POINTER_BUTTON_STATE_RELEASED
+					     : WL_POINTER_BUTTON_STATE_PRESSED);
+			}
+			if ((nm & 1) != (oldmask & 1)) {
+				notify_button(&b->syscons_seat, time, BTN_RIGHT,
+				    (nm & 1) ? WL_POINTER_BUTTON_STATE_RELEASED
+					     : WL_POINTER_BUTTON_STATE_PRESSED);
+			}
+			oldmask = nm;
+		}
+	}
+
+	return 1;
+}
+
+static int
+drm_input_create(struct drm_backend *b)
+{
+	if (b->sysmouse_fd == -1) {
+		b->sysmouse_fd = weston_launcher_open(b->compositor->launcher,
+		    "/dev/sysmouse", O_RDONLY | O_CLOEXEC);
+		if (b->sysmouse_fd < 0)
+			return -1;
+	}
+
+	if (b->kbd_fd == -1) {
+		char *ttyfdstr = getenv("WESTON_TTY_FD");
+		printf("%s: tty fd string: %s\n", __func__,
+		    ttyfdstr != NULL ? ttyfdstr : "NULL");
+		if (ttyfdstr != NULL)
+			b->kbd_fd = atoi(ttyfdstr);
+		if (b->kbd_fd < 0)
+			b->kbd_fd = weston_launcher_open(
+			    b->compositor->launcher, "tty",
+			    O_RDONLY | O_CLOEXEC);
+		if (b->kbd_fd < 0) {
+			close(b->sysmouse_fd);
+			return -1;
+		}
+	}
+
+	int lvl = 1;
+	fcntl(b->sysmouse_fd, F_SETFL, O_NONBLOCK);
+	ioctl(b->sysmouse_fd, MOUSE_SETLEVEL, &lvl);
+
+	fcntl(b->kbd_fd, F_SETFL, O_NONBLOCK);
+	b->kbdst = kbdev_new_state(b->kbd_fd);
+	if (b->kbdst == NULL)
+		return -1;
+
+	weston_seat_init(&b->syscons_seat, b->compositor, "syscons");
+
+	weston_seat_init_pointer(&b->syscons_seat);
+	weston_seat_init_keyboard(&b->syscons_seat, NULL);
+
+	notify_motion_absolute(&b->syscons_seat, weston_compositor_get_time(),
+	    50, 50);
+
+	b->sysmouse_source = wl_event_loop_add_fd(
+	    wl_display_get_event_loop(b->compositor->wl_display),
+	    b->sysmouse_fd, WL_EVENT_READABLE, drm_sysmouse_handler, b);
+
+	b->kbd_source = wl_event_loop_add_fd(
+	    wl_display_get_event_loop(b->compositor->wl_display),
+	    b->kbd_fd, WL_EVENT_READABLE, drm_kbd_handler, b);
+
+	return 0;
+}
+#endif
 
 static void
 drm_restore(struct weston_compositor *ec)
@@ -2705,9 +2924,16 @@
 {
 	struct drm_backend *b = (struct drm_backend *) ec->backend;
 
+#if defined(__FreeBSD__)
+	wl_event_source_remove(b->sysmouse_source);
+	wl_event_source_remove(b->kbd_source);
+	close(b->sysmouse_fd);
+	close(b->kbd_fd);
+#else
 	udev_input_destroy(&b->input);
 
 	wl_event_source_remove(b->udev_drm_source);
+#endif
 	wl_event_source_remove(b->drm_source);
 
 	destroy_sprites(b);
@@ -2749,9 +2975,10 @@
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
@@ -2769,10 +2996,18 @@
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
@@ -2807,9 +3042,12 @@
 {
 	struct weston_compositor *compositor = data;
 
+	weston_log("trying to vt switch to %d\n", key - KEY_F1 + 1);
+
 	weston_launcher_activate_vt(compositor->launcher, key - KEY_F1 + 1);
 }
 
+#if !defined(__FreeBSD__)
 /*
  * Find primary GPU
  * Some systems may have multiple DRM devices attached to a single seat. This
@@ -2865,6 +3103,7 @@
 	udev_enumerate_unref(e);
 	return drm_device;
 }
+#endif
 
 static void
 planes_binding(struct weston_keyboard *keyboard, uint32_t time, uint32_t key,
@@ -2925,7 +3164,7 @@
 	ret = vaapi_recorder_frame(output->recorder, fd,
 				   output->current->stride);
 	if (ret < 0) {
-		weston_log("[libva recorder] aborted: %m\n");
+		weston_log("[libva recorder] aborted: %s\n", strerror(errno));
 		recorder_destroy(output);
 	}
 }
@@ -3059,17 +3298,23 @@
 {
 	struct drm_backend *b;
 	struct weston_config_section *section;
+#if !defined(__FreeBSD__)
 	struct udev_device *drm_device;
+#endif
 	struct wl_event_loop *loop;
 	const char *path;
 	uint32_t key;
 
 	weston_log("initializing drm backend\n");
+	loop = wl_display_get_event_loop(compositor->wl_display);
 
 	b = zalloc(sizeof *b);
 	if (b == NULL)
 		return NULL;
 
+	b->sysmouse_fd = -1;
+	b->kbd_fd = -1;
+
 	/*
 	 * KMS support for hardware planes cannot properly synchronize
 	 * without nuclear page flip. Without nuclear/atomic, hw plane
@@ -3100,23 +3345,33 @@
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
@@ -3138,7 +3393,7 @@
 
 	b->prev_state = WESTON_COMPOSITOR_ACTIVE;
 
-	for (key = KEY_F1; key < KEY_F9; key++)
+	for (key = KEY_F1; key <= KEY_F12; key++)
 		weston_compositor_add_key_binding(compositor, key,
 						  MODIFIER_CTRL | MODIFIER_ALT,
 						  switch_vt_binding, compositor);
@@ -3146,13 +3401,21 @@
 	wl_list_init(&b->sprite_list);
 	create_sprites(b);
 
+#if defined(__FreeBSD__)
+	if (drm_input_create(b) < 0) {
+#else
 	if (udev_input_init(&b->input,
 			    compositor, b->udev, param->seat_id) < 0) {
+#endif
 		weston_log("failed to create input devices\n");
 		goto err_sprite;
 	}
 
+#if defined(__FreeBSD__)
+	if (create_outputs(b, param->connector) < 0) {
+#else
 	if (create_outputs(b, param->connector, drm_device) < 0) {
+#endif
 		weston_log("failed to create output for %s\n", path);
 		goto err_udev_input;
 	}
@@ -3164,11 +3427,11 @@
 
 	path = NULL;
 
-	loop = wl_display_get_event_loop(compositor->wl_display);
 	b->drm_source =
 		wl_event_loop_add_fd(loop, b->drm.fd,
 				     WL_EVENT_READABLE, on_drm_input, b);
 
+#if !defined(__FreeBSD__)
 	b->udev_monitor = udev_monitor_new_from_netlink(b->udev, "udev");
 	if (b->udev_monitor == NULL) {
 		weston_log("failed to intialize udev monitor\n");
@@ -3187,6 +3450,7 @@
 	}
 
 	udev_device_unref(drm_device);
+#endif
 
 	weston_compositor_add_debug_binding(compositor, KEY_O,
 					    planes_binding, b);
@@ -3209,22 +3473,30 @@
 
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
+#endif
 err_sprite:
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
@@ -3241,7 +3513,9 @@
 
 	const struct weston_option drm_options[] = {
 		{ WESTON_OPTION_INTEGER, "connector", 0, &param.connector },
+#if !defined(__FreeBSD__)
 		{ WESTON_OPTION_STRING, "seat", 0, &param.seat_id },
+#endif
 		{ WESTON_OPTION_INTEGER, "tty", 0, &param.tty },
 		{ WESTON_OPTION_BOOLEAN, "current-mode", 0, &option_current_mode },
 		{ WESTON_OPTION_BOOLEAN, "use-pixman", 0, &param.use_pixman },
