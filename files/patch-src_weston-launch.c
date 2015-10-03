--- src/weston-launch.c.orig	2015-08-11 00:28:46 +0200
+++ src/weston-launch.c
@@ -33,7 +33,7 @@
 #include <poll.h>
 #include <errno.h>
 
-#include <error.h>
+#include <err.h>
 #include <getopt.h>
 
 #include <sys/types.h>
@@ -41,14 +41,24 @@
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
+#include <termios.h>
+#include <sys/consio.h>
+#include <sys/kbio.h>
+#else
 #include <linux/vt.h>
 #include <linux/major.h>
 #include <linux/kd.h>
+#endif
 
 #include <pwd.h>
 #include <grp.h>
@@ -60,8 +70,11 @@
 
 #include "weston-launch.h"
 
+//#if !defined(__FreeBSD__)
 #define DRM_MAJOR 226
+//#endif
 
+#if !defined(__FreeBSD__)
 #ifndef KDSKBMUTE
 #define KDSKBMUTE	0x4B51
 #endif
@@ -69,6 +82,7 @@
 #ifndef EVIOCREVOKE
 #define EVIOCREVOKE _IOW('E', 0x91, int)
 #endif
+#endif
 
 #define MAX_ARGV_SIZE 256
 
@@ -100,10 +114,15 @@
 	int sock[2];
 	int drm_fd;
 	int last_input_fd;
+	char *input_path[16];
 	int kb_mode;
 	struct passwd *pw;
 
+#if defined(__FreeBSD__)
+	struct event_base *evbase;
+#else
 	int signalfd;
+#endif
 
 	pid_t child;
 	int verbose;
@@ -226,10 +245,10 @@
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
@@ -261,9 +280,11 @@
 	ret = sigprocmask(SIG_BLOCK, &mask, NULL);
 	assert(ret == 0);
 
+#if !defined(__FreeBSD__)
 	wl->signalfd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
 	if (wl->signalfd < 0)
 		return -errno;
+#endif
 
 	return 0;
 }
@@ -300,6 +321,8 @@
 	struct iovec iov;
 	struct weston_launcher_open *message;
 	union cmsg_data *data;
+	char *path = NULL;
+	char filename[16];
 
 	message = msg->msg_iov->iov_base;
 	if ((size_t)len < sizeof(*message))
@@ -308,28 +331,51 @@
 	/* Ensure path is null-terminated */
 	((char *) message)[len-1] = '\0';
 
-	fd = open(message->path, message->flags);
+	if (strcmp(message->path, "tty") == 0) {
+		if (wl->ttynr < 0) {
+			path = strdup("/dev/tty");
+		} else {
+			snprintf(filename, 15, "/dev/ttyv%d", wl->ttynr - 1);
+			path = strdup(filename);
+		}
+	} else {
+		path = strdup(message->path);
+	}
+	fd = open(path, message->flags);
 	if (fd < 0) {
 		fprintf(stderr, "Error opening device %s: %m\n",
-			message->path);
+			path);
 		goto err0;
 	}
+	printf("opened device %s to fd %d\n", path, fd);
 
 	if (fstat(fd, &s) < 0) {
 		close(fd);
 		fd = -1;
-		fprintf(stderr, "Failed to stat %s\n", message->path);
+		fprintf(stderr, "Failed to stat %s\n", path);
 		goto err0;
 	}
 
-	if (major(s.st_rdev) != INPUT_MAJOR &&
-	    major(s.st_rdev) != DRM_MAJOR) {
+#if defined(__FreeBSD__)
+	if (major(s.st_rdev) != DRM_MAJOR) {
+		if (fd-3 >= 16) {
+			warnx("no more than 16 input devices allowed");
+			close(fd);
+			fd = -1;
+			goto err0;
+		}
+	}
+#endif
+
+#if !defined(__FreeBSD__)
+	if (major(s.st_rdev) != DRM_MAJOR) {
 		close(fd);
 		fd = -1;
 		fprintf(stderr, "Device %s is not an input or drm device\n",
 			message->path);
 		goto err0;
 	}
+#endif
 
 err0:
 	memset(&nmsg, 0, sizeof nmsg);
@@ -352,31 +398,53 @@
 
 	if (wl->verbose)
 		fprintf(stderr, "weston-launch: opened %s: ret: %d, fd: %d\n",
-			message->path, ret, fd);
+			path, ret, fd);
 	do {
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
+	if (fd != -1 &&
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
 
@@ -392,17 +460,32 @@
 		len = recvmsg(wl->sock[0], &msg, 0);
 	} while (len < 0 && errno == EINTR);
 
-	if (len < 1)
+	if (len < 1) {
+#if defined(__FreeBSD__)
+		if (errno != EAGAIN)
+			event_base_loopbreak(wl->evbase);
+		return;
+#else
 		return -1;
+#endif
+	}
 
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
@@ -411,7 +494,9 @@
 	struct vt_mode mode = { 0 };
 	int err;
 
+#if !defined(__FreeBSD__)
 	close(wl->signalfd);
+#endif
 	close(wl->sock[0]);
 
 	if (wl->new_user) {
@@ -422,8 +507,12 @@
 		pam_end(wl->ph, err);
 	}
 
+#if defined(__FreeBSD__)
+	if (ioctl(wl->tty, KDSKBMODE, wl->kb_mode))
+#else
 	if (ioctl(wl->tty, KDSKBMUTE, 0) &&
 	    ioctl(wl->tty, KDSKBMODE, wl->kb_mode))
+#endif
 		fprintf(stderr, "failed to restore keyboard mode: %m\n");
 
 	if (ioctl(wl->tty, KDSETMODE, KD_TEXT))
@@ -447,28 +536,55 @@
 	struct stat s;
 	int fd;
 
+	printf("%s: closing input fds\n", __func__);
+
 	for (fd = 3; fd <= wl->last_input_fd; fd++) {
+#if defined(__FreeBSD__)
+		if (fstat(fd, &s) == 0) {
+			if (wl->input_path[fd-3] != NULL) {
+				printf("revoking access to device %s\n", wl->input_path[fd-3]);
+				revoke(wl->input_path[fd-3]);
+				free(wl->input_path[fd-3]);
+				wl->input_path[fd-3] = NULL;
+			}
+#else
 		if (fstat(fd, &s) == 0 && major(s.st_rdev) == INPUT_MAJOR) {
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
+
+	warnx("%s: running", __func__);
 
+#if defined(__FreeBSD__)
+	switch (fd) {
+#else
 	switch (sig.ssi_signo) {
+#endif
 	case SIGCHLD:
 		pid = waitpid(-1, &status, 0);
 		if (pid == wl->child) {
@@ -491,33 +607,51 @@
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
+		warnx("%s: leaving vt", __func__);
 		send_reply(wl, WESTON_LAUNCHER_DEACTIVATE);
 		close_input_fds(wl);
 		drmDropMaster(wl->drm_fd);
 		ioctl(wl->tty, VT_RELDISP, 1);
 		break;
 	case SIGUSR2:
+		warnx("%s: entering vt", __func__);
 		ioctl(wl->tty, VT_RELDISP, VT_ACKACQ);
 		drmSetMaster(wl->drm_fd);
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
 
+	wl->ttynr = -1;
+
 	if (!wl->new_user) {
 		wl->tty = STDIN_FILENO;
 	} else if (tty) {
@@ -527,52 +661,89 @@
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
+		snprintf(filename, sizeof filename, "/dev/ttyv%d", wl->ttynr - 1);
+		printf("%s: using tty %s\n", __func__, filename);
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
+
+#if defined(__FreeBSD__)
+	printf("%s: wl->ttynr = %d\n", __func__, wl->ttynr);
+	if (wl->ttynr > 0) {
+		if (ioctl(wl->tty, VT_ACTIVATE, wl->ttynr) != 0)
+			err(1, "VT_ACTIVATE");
+		if (ioctl(wl->tty, VT_WAITACTIVE, wl->ttynr) != 0)
+			err(1, "VT_ACTIVATE");
+	}
+#endif
 
 	if (ioctl(wl->tty, KDGKBMODE, &wl->kb_mode))
-		error(1, errno, "failed to get current keyboard mode: %m\n");
+		err(1, "failed to get current keyboard mode");
 
+#if defined(__FreeBSD__)
+	if (ioctl(wl->tty, KDSKBMODE, K_CODE))
+		err(1, "failed to set K_CODE keyboard mode");
+#else
 	if (ioctl(wl->tty, KDSKBMUTE, 1) &&
 	    ioctl(wl->tty, KDSKBMODE, K_OFF))
-		error(1, errno, "failed to set K_OFF keyboard mode: %m\n");
+		err(1, "failed to set K_OFF keyboard mode");
+#endif
 
 	if (ioctl(wl->tty, KDSETMODE, KD_GRAPHICS))
-		error(1, errno, "failed to set KD_GRAPHICS mode on tty: %m\n");
+		err(1, "failed to set KD_GRAPHICS mode on tty");
+
+#if defined(__FreeBSD__)
+	/* Put the tty into raw mode */
+	struct termios tios;
+	tcgetattr(wl->tty, &tios);
+	cfmakeraw(&tios);
+	tcsetattr(wl->tty, TCSAFLUSH, &tios);
+#endif
 
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
@@ -586,28 +757,37 @@
 
 	if (wl->tty != STDIN_FILENO) {
 		if (setsid() < 0)
-			error(1, errno, "setsid failed");
+			err(1, "setsid");
 		if (ioctl(wl->tty, TIOCSCTTY, 0) < 0)
-			error(1, errno, "TIOCSCTTY failed - tty is in use");
+			err(1, "TIOCSCTTY failed - tty is in use");
 	}
 
 	term = getenv("TERM");
+	char *xdg_runtime = getenv("XDG_RUNTIME_DIR");
+#if defined(__FreeBSD__)
+	extern char **environ;
+	environ = NULL;
+#else
 	clearenv();
+#endif
 	if (term)
 		setenv("TERM", term, 1);
 	setenv("USER", wl->pw->pw_name, 1);
 	setenv("LOGNAME", wl->pw->pw_name, 1);
 	setenv("HOME", wl->pw->pw_dir, 1);
 	setenv("SHELL", wl->pw->pw_shell, 1);
+	setenv("XDG_RUNTIME_DIR", xdg_runtime, 1);
 
 	env = pam_getenvlist(wl->ph);
 	if (env) {
 		for (i = 0; env[i]; ++i) {
 			if (putenv(env[i]) != 0)
-				error(0, 0, "putenv %s failed", env[i]);
+				errx(0, "putenv %s failed", env[i]);
 		}
 		free(env);
 	}
+
+	chdir(wl->pw->pw_dir);
 }
 
 static void
@@ -618,7 +798,7 @@
 	    initgroups(wl->pw->pw_name, wl->pw->pw_gid) < 0 ||
 #endif
 	    setuid(wl->pw->pw_uid) < 0)
-		error(1, errno, "dropping privileges failed");
+		err(1, "dropping privileges failed");
 }
 
 static void
@@ -648,6 +828,17 @@
 	sigaddset(&mask, SIGINT);
 	sigprocmask(SIG_UNBLOCK, &mask, NULL);
 
+#if defined(__FreeBSD__)
+	child_argv[0] = "-/bin/sh";
+	child_argv[1] = "-c";
+	child_argv[2] = BINDIR "/weston \"$@\"";
+	child_argv[3] = "weston";
+	for (i = 0; i < argc; ++i)
+		child_argv[4 + i] = argv[i];
+	child_argv[4 + i] = NULL;
+
+	execv("/bin/sh", child_argv);
+#else
 	child_argv[0] = "/bin/sh";
 	child_argv[1] = "-l";
 	child_argv[2] = "-c";
@@ -658,7 +849,8 @@
 	child_argv[5 + i] = NULL;
 
 	execv(child_argv[0], child_argv);
-	error(1, errno, "exec failed");
+#endif
+	err(1, "exec failed");
 }
 
 static void
@@ -692,7 +884,7 @@
 		case 'u':
 			wl.new_user = optarg;
 			if (getuid() != 0)
-				error(1, 0, "Permission denied. -u allowed for root only");
+				errx(1, "Permission denied. -u allowed for root only");
 			break;
 		case 't':
 			tty = optarg;
@@ -707,17 +899,17 @@
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
@@ -739,7 +931,7 @@
 
 	wl.child = fork();
 	if (wl.child == -1)
-		error(EXIT_FAILURE, errno, "fork failed");
+		err(EXIT_FAILURE, "fork failed");
 
 	if (wl.child == 0)
 		launch_compositor(&wl, argc - optind, argv + optind);
@@ -748,6 +940,45 @@
 	if (wl.tty != STDIN_FILENO)
 		close(wl.tty);
 
+#if defined(__FreeBSD__)
+	wl.evbase = event_base_new();
+	struct event *sockev = event_new(wl.evbase, wl.sock[0],
+	    EV_READ | EV_PERSIST, handle_socket_msg, &wl);
+	struct event *chldev = evsignal_new(wl.evbase, SIGCHLD, handle_signal,
+	    &wl);
+	struct event *intev = evsignal_new(wl.evbase, SIGINT, handle_signal,
+	    &wl);
+	struct event *termev = evsignal_new(wl.evbase, SIGTERM, handle_signal,
+	    &wl);
+	struct event *usr1ev = evsignal_new(wl.evbase, SIGUSR1, handle_signal,
+	    &wl);
+	struct event *usr2ev = evsignal_new(wl.evbase, SIGUSR2, handle_signal,
+	    &wl);
+
+	event_add(sockev, NULL);
+	event_add(chldev, NULL);
+	event_add(intev, NULL);
+	event_add(termev, NULL);
+	event_add(usr1ev, NULL);
+	event_add(usr2ev, NULL);
+
+	event_base_loop(wl.evbase, 0);
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
+	event_base_free(wl.evbase);
+#else
 	while (1) {
 		struct pollfd fds[2];
 		int n;
@@ -759,12 +990,13 @@
 
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
