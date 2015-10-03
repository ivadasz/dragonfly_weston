--- src/weston-launch.c.orig	2015-08-11 00:28:46 +0200
+++ src/weston-launch.c
@@ -33,7 +33,7 @@
 #include <poll.h>
 #include <errno.h>
 
-#include <error.h>
+#include <err.h>
 #include <getopt.h>
 
 #include <sys/types.h>
@@ -41,14 +41,23 @@
 #include <sys/stat.h>
 #include <sys/wait.h>
 #include <sys/socket.h>
+#if defined(__FreeBSD__)
+#include <event2/event.h>
+#else
 #include <sys/signalfd.h>
+#endif
 #include <signal.h>
 #include <unistd.h>
 #include <fcntl.h>
 
+#if defined(__FreeBSD__)
+#include <sys/consio.h>
+#include <sys/kbio.h>
+#else
 #include <linux/vt.h>
 #include <linux/major.h>
 #include <linux/kd.h>
+#endif
 
 #include <pwd.h>
 #include <grp.h>
@@ -100,10 +109,13 @@
 	int sock[2];
 	int drm_fd;
 	int last_input_fd;
+	char *input_path[16];
 	int kb_mode;
 	struct passwd *pw;
 
+#if !defined(__FreeBSD__)
 	int signalfd;
+#endif
 
 	pid_t child;
 	int verbose;
@@ -226,10 +238,10 @@
 setup_launcher_socket(struct weston_launch *wl)
 {
 	if (socketpair(AF_LOCAL, SOCK_SEQPACKET, 0, wl->sock) < 0)
-		error(1, errno, "socketpair failed");
+		err(1, "socketpair");
 
 	if (fcntl(wl->sock[0], F_SETFD, FD_CLOEXEC) < 0)
-		error(1, errno, "fcntl failed");
+		err(1, "fcntl");
 
 	return 0;
 }
@@ -261,9 +273,11 @@
 	ret = sigprocmask(SIG_BLOCK, &mask, NULL);
 	assert(ret == 0);
 
+#if !defined(__FreeBSD__)
 	wl->signalfd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
 	if (wl->signalfd < 0)
 		return -errno;
+#endif
 
 	return 0;
 }
@@ -300,6 +314,7 @@
 	struct iovec iov;
 	struct weston_launcher_open *message;
 	union cmsg_data *data;
+	char *path = NULL;
 
 	message = msg->msg_iov->iov_base;
 	if ((size_t)len < sizeof(*message))
@@ -314,6 +329,8 @@
 			message->path);
 		goto err0;
 	}
+	printf("opened device %s to fd %d\n", message->path, fd);
+	path = strdup(message->path);
 
 	if (fstat(fd, &s) < 0) {
 		close(fd);
@@ -322,6 +339,16 @@
 		goto err0;
 	}
 
+#if defined(__FreeBSD__)
+	if (major(s.st_rdev) != DRM_MAJOR) {
+		if (fd-3 >= 16) {
+			warnx("no more than 16 input devices allowed");
+			close(fd);
+			fd = -1;
+			goto err0;
+		}
+	}
+#else
 	if (major(s.st_rdev) != INPUT_MAJOR &&
 	    major(s.st_rdev) != DRM_MAJOR) {
 		close(fd);
@@ -330,6 +357,7 @@
 			message->path);
 		goto err0;
 	}
+#endif
 
 err0:
 	memset(&nmsg, 0, sizeof nmsg);
@@ -357,26 +385,48 @@
 		len = sendmsg(wl->sock[0], &nmsg, 0);
 	} while (len < 0 && errno == EINTR);
 
-	if (len < 0)
+	if (len < 0) {
+#if defined(__FreeBSD__)
+		if (path != NULL)
+			free(path);
+#endif
 		return -1;
+	}
 
 	if (fd != -1 && major(s.st_rdev) == DRM_MAJOR)
 		wl->drm_fd = fd;
+#if defined(__FreeBSD__)
+	else if (fd != -1 &&
+	    wl->last_input_fd < fd) {
+		wl->last_input_fd = fd;
+		wl->input_path[fd-3] = path;
+	}
+#else
 	if (fd != -1 && major(s.st_rdev) == INPUT_MAJOR &&
 	    wl->last_input_fd < fd)
 		wl->last_input_fd = fd;
+#endif
 
 	return 0;
 }
 
+#if defined(__FreeBSD__)
+static void
+handle_socket_msg(evutil_socket_t fd, short events, void *arg)
+{
+	struct weston_launch *wl = arg;
+#else
 static int
 handle_socket_msg(struct weston_launch *wl)
 {
+#endif
 	char control[CMSG_SPACE(sizeof(int))];
 	char buf[BUFSIZ];
 	struct msghdr msg;
 	struct iovec iov;
+#if !defined(__FreeBSD__)
 	int ret = -1;
+#endif
 	ssize_t len;
 	struct weston_launcher_message *message;
 
@@ -393,16 +443,28 @@
 	} while (len < 0 && errno == EINTR);
 
 	if (len < 1)
+#if defined(__FreeBSD__)
+		return;
+#else
 		return -1;
+#endif
 
 	message = (void *) buf;
 	switch (message->opcode) {
 	case WESTON_LAUNCHER_OPEN:
+#if defined(__FreeBSD__)
+		handle_open(wl, &msg, len);
+#else
 		ret = handle_open(wl, &msg, len);
+#endif
 		break;
 	}
 
+#if defined(__FreeBSD__)
+	return;
+#else
 	return ret;
+#endif
 }
 
 static void
@@ -411,7 +473,9 @@
 	struct vt_mode mode = { 0 };
 	int err;
 
+#if !defined(__FreeBSD__)
 	close(wl->signalfd);
+#endif
 	close(wl->sock[0]);
 
 	if (wl->new_user) {
@@ -448,27 +512,48 @@
 	int fd;
 
 	for (fd = 3; fd <= wl->last_input_fd; fd++) {
+#if defined(__FreeBSD__)
+		if (fstat(fd, &s) == 0) {
+#else
 		if (fstat(fd, &s) == 0 && major(s.st_rdev) == INPUT_MAJOR) {
+#endif
+#if defined(__FreeBSD__)
+			revoke(wl->input_path[fd-3]);
+#else
 			/* EVIOCREVOKE may fail if the kernel doesn't
 			 * support it, but all we can do is ignore it. */
 			ioctl(fd, EVIOCREVOKE, 0);
+#endif
 			close(fd);
 		}
 	}
 }
 
+#if defined(__FreeBSD__)
+static void
+handle_signal(evutil_socket_t fd, short events, void *arg)
+{
+	struct weston_launch *wl = arg;
+#else
 static int
 handle_signal(struct weston_launch *wl)
 {
 	struct signalfd_siginfo sig;
+#endif
 	int pid, status, ret;
 
+#if !defined(__FreeBSD__)
 	if (read(wl->signalfd, &sig, sizeof sig) != sizeof sig) {
-		error(0, errno, "reading signalfd failed");
+		err(0, "reading signalfd failed");
 		return -1;
 	}
+#endif
 
+#if defined(__FreeBSD__)
+	switch (fd) {
+#else
 	switch (sig.ssi_signo) {
+#endif
 	case SIGCHLD:
 		pid = waitpid(-1, &status, 0);
 		if (pid == wl->child) {
@@ -491,7 +576,11 @@
 	case SIGTERM:
 	case SIGINT:
 		if (wl->child)
+#if defined(__FreeBSD__)
+			kill(wl->child, fd);
+#else
 			kill(wl->child, sig.ssi_signo);
+#endif
 		break;
 	case SIGUSR1:
 		send_reply(wl, WESTON_LAUNCHER_DEACTIVATE);
@@ -505,16 +594,26 @@
 		send_reply(wl, WESTON_LAUNCHER_ACTIVATE);
 		break;
 	default:
+#if defined(__FreeBSD__)
+		return;
+#else
 		return -1;
+#endif
 	}
 
+#if defined(__FreeBSD__)
+	return;
+#else
 	return 0;
+#endif
 }
 
 static int
 setup_tty(struct weston_launch *wl, const char *tty)
 {
+#if !defined(__FreeBSD__)
 	struct stat buf;
+#endif
 	struct vt_mode mode = { 0 };
 	char *t;
 
@@ -527,52 +626,67 @@
 		else
 			wl->tty = open(tty, O_RDWR | O_NOCTTY);
 	} else {
+#if defined(__FreeBSD__)
+		int tty0 = open("/dev/ttyv0", O_WRONLY | O_CLOEXEC);
+#else
 		int tty0 = open("/dev/tty0", O_WRONLY | O_CLOEXEC);
+#endif
 		char filename[16];
 
 		if (tty0 < 0)
-			error(1, errno, "could not open tty0");
+			err(1, "could not open tty0");
 
 		if (ioctl(tty0, VT_OPENQRY, &wl->ttynr) < 0 || wl->ttynr == -1)
-			error(1, errno, "failed to find non-opened console");
+			err(1, "failed to find non-opened console");
 
+#if defined(__FreeBSD__)
+		snprintf(filename, sizeof filename, "/dev/ttyv%d", wl->ttynr);
+#else
 		snprintf(filename, sizeof filename, "/dev/tty%d", wl->ttynr);
+#endif
 		wl->tty = open(filename, O_RDWR | O_NOCTTY);
 		close(tty0);
 	}
 
 	if (wl->tty < 0)
-		error(1, errno, "failed to open tty");
+		err(1, "failed to open tty");
 
+#if !defined(__FreeBSD__)
 	if (fstat(wl->tty, &buf) == -1 ||
 	    major(buf.st_rdev) != TTY_MAJOR || minor(buf.st_rdev) == 0)
-		error(1, 0, "weston-launch must be run from a virtual terminal");
+		errx(1, "weston-launch must be run from a virtual terminal");
 
 	if (tty) {
 		if (fstat(wl->tty, &buf) < 0)
-			error(1, errno, "stat %s failed", tty);
+			err(1, "stat: %s", tty);
 
 		if (major(buf.st_rdev) != TTY_MAJOR)
-			error(1, 0, "invalid tty device: %s", tty);
+			errx(1, "invalid tty device: %s", tty);
 
 		wl->ttynr = minor(buf.st_rdev);
 	}
+#endif
 
 	if (ioctl(wl->tty, KDGKBMODE, &wl->kb_mode))
-		error(1, errno, "failed to get current keyboard mode: %m\n");
+		err(1, "failed to get current keyboard mode");
 
+#if !defined(__FreeBSD__)
 	if (ioctl(wl->tty, KDSKBMUTE, 1) &&
 	    ioctl(wl->tty, KDSKBMODE, K_OFF))
-		error(1, errno, "failed to set K_OFF keyboard mode: %m\n");
+		err(1, "failed to set K_OFF keyboard mode");
+#endif
 
 	if (ioctl(wl->tty, KDSETMODE, KD_GRAPHICS))
-		error(1, errno, "failed to set KD_GRAPHICS mode on tty: %m\n");
+		err(1, "failed to set KD_GRAPHICS mode on tty");
 
 	mode.mode = VT_PROCESS;
 	mode.relsig = SIGUSR1;
 	mode.acqsig = SIGUSR2;
+#if defined(__FreeBSD__)
+	mode.frsig = SIGIO; /* not used, but has to be set anyway */
+#endif
 	if (ioctl(wl->tty, VT_SETMODE, &mode) < 0)
-		error(1, errno, "failed to take control of vt handling\n");
+		err(1, "failed to take control of vt handling");
 
 	return 0;
 }
@@ -586,13 +700,15 @@
 
 	if (wl->tty != STDIN_FILENO) {
 		if (setsid() < 0)
-			error(1, errno, "setsid failed");
+			err(1, "setsid");
 		if (ioctl(wl->tty, TIOCSCTTY, 0) < 0)
-			error(1, errno, "TIOCSCTTY failed - tty is in use");
+			err(1, "TIOCSCTTY failed - tty is in use");
 	}
 
 	term = getenv("TERM");
+#if !defined(__FreeBSD__)
 	clearenv();
+#endif
 	if (term)
 		setenv("TERM", term, 1);
 	setenv("USER", wl->pw->pw_name, 1);
@@ -604,7 +720,7 @@
 	if (env) {
 		for (i = 0; env[i]; ++i) {
 			if (putenv(env[i]) != 0)
-				error(0, 0, "putenv %s failed", env[i]);
+				errx(0, "putenv %s failed", env[i]);
 		}
 		free(env);
 	}
@@ -618,7 +734,7 @@
 	    initgroups(wl->pw->pw_name, wl->pw->pw_gid) < 0 ||
 #endif
 	    setuid(wl->pw->pw_uid) < 0)
-		error(1, errno, "dropping privileges failed");
+		err(1, "dropping privileges failed");
 }
 
 static void
@@ -648,6 +764,15 @@
 	sigaddset(&mask, SIGINT);
 	sigprocmask(SIG_UNBLOCK, &mask, NULL);
 
+#if defined(__FreeBSD__)
+	child_argv[0] = "/bin/sh";
+	child_argv[1] = "-c";
+	child_argv[2] = BINDIR "/weston \"$@\"";
+	child_argv[3] = "weston";
+	for (i = 0; i < argc; ++i)
+		child_argv[4 + i] = argv[i];
+	child_argv[4 + i] = NULL;
+#else
 	child_argv[0] = "/bin/sh";
 	child_argv[1] = "-l";
 	child_argv[2] = "-c";
@@ -656,9 +781,10 @@
 	for (i = 0; i < argc; ++i)
 		child_argv[5 + i] = argv[i];
 	child_argv[5 + i] = NULL;
+#endif
 
 	execv(child_argv[0], child_argv);
-	error(1, errno, "exec failed");
+	err(1, "exec failed");
 }
 
 static void
@@ -692,7 +818,7 @@
 		case 'u':
 			wl.new_user = optarg;
 			if (getuid() != 0)
-				error(1, 0, "Permission denied. -u allowed for root only");
+				errx(1, "Permission denied. -u allowed for root only");
 			break;
 		case 't':
 			tty = optarg;
@@ -707,17 +833,17 @@
 	}
 
 	if ((argc - optind) > (MAX_ARGV_SIZE - 6))
-		error(1, E2BIG, "Too many arguments to pass to weston");
+		err(1, "Too many arguments to pass to weston");
 
 	if (wl.new_user)
 		wl.pw = getpwnam(wl.new_user);
 	else
 		wl.pw = getpwuid(getuid());
 	if (wl.pw == NULL)
-		error(1, errno, "failed to get username");
+		err(1, "failed to get username");
 
 	if (!weston_launch_allowed(&wl))
-		error(1, 0, "Permission denied. You should either:\n"
+		err(1, "Permission denied. You should either:\n"
 #ifdef HAVE_SYSTEMD_LOGIN
 		      " - run from an active and local (systemd) session.\n"
 #else
@@ -739,7 +865,7 @@
 
 	wl.child = fork();
 	if (wl.child == -1)
-		error(EXIT_FAILURE, errno, "fork failed");
+		err(EXIT_FAILURE, "fork failed");
 
 	if (wl.child == 0)
 		launch_compositor(&wl, argc - optind, argv + optind);
@@ -748,6 +874,45 @@
 	if (wl.tty != STDIN_FILENO)
 		close(wl.tty);
 
+#if defined(__FreeBSD__)
+	struct event_base *evbase = event_base_new();
+	struct event *sockev = event_new(evbase, wl.sock[0],
+	    EV_READ | EV_PERSIST, handle_socket_msg, &wl);
+	struct event *chldev = evsignal_new(evbase, SIGCHLD, handle_signal,
+	    &wl);
+	struct event *intev = evsignal_new(evbase, SIGINT, handle_signal,
+	    &wl);
+	struct event *termev = evsignal_new(evbase, SIGTERM, handle_signal,
+	    &wl);
+	struct event *usr1ev = evsignal_new(evbase, SIGUSR1, handle_signal,
+	    &wl);
+	struct event *usr2ev = evsignal_new(evbase, SIGUSR2, handle_signal,
+	    &wl);
+
+	event_add(sockev, NULL);
+	event_add(chldev, NULL);
+	event_add(intev, NULL);
+	event_add(termev, NULL);
+	event_add(usr1ev, NULL);
+	event_add(usr2ev, NULL);
+
+	event_base_loop(evbase, 0);
+
+	event_del(sockev);
+	event_del(chldev);
+	event_del(intev);
+	event_del(termev);
+	event_del(usr1ev);
+	event_del(usr2ev);
+
+	event_free(chldev);
+	event_free(intev);
+	event_free(termev);
+	event_free(usr1ev);
+	event_free(usr2ev);
+	event_free(sockev);
+	event_base_free(evbase);
+#else
 	while (1) {
 		struct pollfd fds[2];
 		int n;
@@ -759,12 +924,13 @@
 
 		n = poll(fds, 2, -1);
 		if (n < 0)
-			error(0, errno, "poll failed");
+			err(0, "poll failed");
 		if (fds[0].revents & POLLIN)
 			handle_socket_msg(&wl);
 		if (fds[1].revents)
 			handle_signal(&wl);
 	}
+#endif
 
 	return 0;
 }
