PORTNAME=	weston
PORTVERSION=	1.9.0
CATEGORIES=	graphics
MASTER_SITES=	http://wayland.freedesktop.org/releases/
MAINTAINER=	imre@vdsz.com
COMMENT=	Wayland Default Compositor

LICENSE=	MIT

USES=		alias execinfo gmake jpeg tar:xz
WRKSRC=		${WRKDIR}/${PORTNAME}-${PORTVERSION}
CPPFLAGS+=	-I${LOCALBASE}/include
LDFLAGS+=	-L${LOCALBASE}/lib

LIB_DEPENDS=	libxkbcommon.so:${PORTSDIR}/x11/libxkbcommon		\
		libexecinfo.so:${PORTSDIR}/devel/libexecinfo		\
		libpixman-1.so:${PORTSDIR}/x11/pixman			\
		libwayland-server.so:${PORTSDIR}/graphics/wayland	\
		libwayland-client.so:${PORTSDIR}/graphics/wayland	\
		libwayland-cursor.so:${PORTSDIR}/graphics/wayland	\
		libevent.so:${PORTSDIR}/devel/libevent2			\
		libffi.so:${PORTSDIR}/devel/libffi

# XXX drm backend not available yet
LIB_DEPENDS+=	libgbm.so:${PORTSDIR}/graphics/gbm			\
		libdrm.so:${PORTSDIR}/graphics/libdrm

LIBS+=		-lexecinfo
# XXX Rather add these to the Makefile.in for the relevant binaries only
LIBS+=		-lkbdev
LIBS+=		-ldevattr -lprop

GNU_CONFIGURE=	YES

CONFIGURE_ENV+=		WESTON_NATIVE_BACKEND=x11-backend.so
CONFIGURE_ARGS+=	--with-libevent=${PREFIX}
CONFIGURE_ARGS+=	--disable-egl --enable-weston-launch
CONFIGURE_ARGS+=	--enable-drm-compositor --disable-rpi-compositor
CONFIGURE_ARGS+=	--disable-fbdev-compositor --disable-vaapi-recorder
CONFIGURE_ARGS+=	--disable-dbus --enable-setuid-install

.include <bsd.port.mk>
