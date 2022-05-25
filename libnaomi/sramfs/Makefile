# Please see the README for setting up a valid build environment.

# We only need to build the libnaomi.a library here
all: libnaomisramfs.a

# The source files that make libnaomisramfs.a tick.
SRCS += sramfs.c

# Pick up base makefile rules common to all examples.
include ../../Makefile.base

# Package up all object files, created from the above sources.
libnaomisramfs.a: ${OBJS}
	$(AR) -rcs -o $@ $^

# Package up all the dependencies and such that libnaomi needs and put them
# into our install directory.
.PHONY: install
install: libnaomisramfs.a
	mkdir -p ${NAOMI_SH_BASE}/lib
	cp libnaomisramfs.a ${NAOMI_SH_BASE}/lib

# Include a simple clean target which wipes the build directory
# and kills any library built.
.PHONY: clean
clean:
	rm -rf build
	rm -rf libnaomisramfs.a
