all: build

# Download arguments
BASE_URL = https://github.com/libxmp/libxmp/archive/refs/tags/

NAME = libxmp-libxmp-4.6.2

ARTIFACT = libxmp-4.6.2.tar.gz

# Build arguments
USES_AUTOCONF = 1
GENERATE_CONFIGURE = 1

CONFIGURE_ARGS = --host=sh-elf --enable-static

DEFINES = -DPATH_MAX=244

BUILD_TARGET = lib/libxmp.a

INSTALL_HEADERS = include/xmp.h

include ../Makefile.base
