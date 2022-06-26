all: build

# Download arguments
BASE_URL = https://www.mpg123.de/download/

NAME = mpg123-1.29.3

ARTIFACT = mpg123-1.29.3.tar.bz2

# Build arguments
USES_AUTOCONF = 1

CONFIGURE_ARGS = --host=sh-elf --enable-static

BUILD_TARGET = src/libmpg123/libmpg123.la

INSTALL_LIBS = src/libmpg123/.libs/libmpg123.a

INSTALL_HEADERS = src/libmpg123/mpg123.h src/libmpg123/fmt123.h

include ../Makefile.base
