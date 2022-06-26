all: build

# Download arguments
BASE_URL = https://ftp.osuosl.org/pub/xiph/releases/vorbis/

NAME = libvorbis-1.3.7

ARTIFACT = libvorbis-1.3.7.zip

# Build arguments
USES_AUTOCONF = 1

CONFIGURE_ARGS = --host=sh-elf --enable-static

BUILD_TARGET = all

INSTALL_LIBS = lib/.libs/libvorbis.a

INSTALL_HEADERS = include/vorbis/codec.h include/vorbis/vorbisenc.h include/vorbis/vorbisfile.h

HEADER_INSTALLDIR = vorbis

include ../Makefile.base
