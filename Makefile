PORTNAME=	weston
PORTVERSION=	1.9.0
CATEGORIES=	net
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
		libgbm.so:${PORTSDIR}/graphics/gbm			\
		libdrm.so:${PORTSDIR}/graphics/libdrm			\
		libffi.so:${PORTSDIR}/devel/libffi

LIBS+=		-lexecinfo

GNU_CONFIGURE=	YES

CONFIGURE_ENV+=		WESTON_NATIVE_BACKEND=x11-backend.so
#CONFIGURE_ARGS+=	--with-libevent=${PREFIX}
CONFIGURE_ARGS+=	--disable-egl --disable-weston-launch
CONFIGURE_ARGS+=	--disable-drm-compositor --disable-rpi-compositor
CONFIGURE_ARGS+=	--disable-fbdev-compositor --disable-vaapi-recorder
CONFIGURE_ARGS+=	--disable-dbus --disable-setuid-install

.include <bsd.port.mk>

