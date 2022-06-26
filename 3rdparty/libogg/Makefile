all: build

# Download arguments
BASE_URL = https://ftp.osuosl.org/pub/xiph/releases/ogg/

NAME = libogg-1.3.5

ARTIFACT = libogg-1.3.5.zip

# Build arguments
USES_AUTOCONF = 1

CONFIGURE_ARGS = --host=sh-elf --enable-static

BUILD_TARGET = all

INSTALL_LIBS = src/.libs/libogg.a

INSTALL_HEADERS = include/ogg/ogg.h include/ogg/os_types.h include/ogg/config_types.h

HEADER_INSTALLDIR = ogg

include ../Makefile.base
