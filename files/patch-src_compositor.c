--- src/compositor.c.orig	2015-10-02 22:10:28 +0200
+++ src/compositor.c
@@ -256,7 +256,7 @@
 	 * non-CLOEXEC fd to pass through exec. */
 	clientfd = dup(sockfd);
 	if (clientfd == -1) {
-		weston_log("compositor: dup failed: %m\n");
+		weston_log("compositor: dup failed: %s\n", strerror(errno));
 		return;
 	}
 
@@ -264,8 +264,8 @@
 	setenv("WAYLAND_SOCKET", s, 1);
 
 	if (execl(path, path, NULL) < 0)
-		weston_log("compositor: executing '%s' failed: %m\n",
-			path);
+		weston_log("compositor: executing '%s' failed: %s\n",
+			path, strerror(errno));
 }
 
 WL_EXPORT struct wl_client *
@@ -282,8 +282,8 @@
 
 	if (os_socketpair_cloexec(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
 		weston_log("weston_client_launch: "
-			"socketpair failed while launching '%s': %m\n",
-			path);
+			"socketpair failed while launching '%s': %s\n",
+			path, strerror(errno));
 		return NULL;
 	}
 
@@ -292,7 +292,8 @@
 		close(sv[0]);
 		close(sv[1]);
 		weston_log("weston_client_launch: "
-			"fork failed while launching '%s': %m\n", path);
+			"fork failed while launching '%s': %s\n", path,
+			strerror(errno));
 		return NULL;
 	}
 
@@ -4621,18 +4622,25 @@
 {
 	/* In order of preference */
 	static const clockid_t clocks[] = {
+#if defined(__FreeBSD__)
+		CLOCK_MONOTONIC_FAST,	/* no jumps, may crawl, fast & coarse */
+#else
 		CLOCK_MONOTONIC_RAW,	/* no jumps, no crawling */
 		CLOCK_MONOTONIC_COARSE,	/* no jumps, may crawl, fast & coarse */
+#endif
 		CLOCK_MONOTONIC,	/* no jumps, may crawl */
+#if defined(__FreeBSD__)
+		CLOCK_REALTIME_FAST,	/* may jump and crawl, fast & coarse */
+#else
 		CLOCK_REALTIME_COARSE,	/* may jump and crawl, fast & coarse */
+#endif
 		CLOCK_REALTIME		/* may jump and crawl */
 	};
 	unsigned i;
 
 	for (i = 0; i < ARRAY_LENGTH(clocks); i++)
 		if (weston_compositor_set_presentation_clock(compositor,
-							     clocks[i]) == 0)
-			return 0;
+							     clocks[i]) == 0) return 0;
 
 	weston_log("Error: no suitable presentation clock available.\n");
 
@@ -4666,8 +4674,9 @@
 
 		if (!warned)
 			weston_log("Error: failure to read "
-				   "the presentation clock %#x: '%m' (%d)\n",
-				   compositor->presentation_clock, errno);
+				   "the presentation clock %#lx: '%s' (%d)\n",
+				   compositor->presentation_clock,
+				   strerror(errno), errno);
 		warned = true;
 	}
 }
