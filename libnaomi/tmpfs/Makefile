# Please see the README for setting up a valid build environment.

# We only need to build the libnaomitmpfs.a library here
all: libnaomitmpfs.a

# The source files that make libnaomitmpfs.a tick.
SRCS += tmpfs.c

# Pick up base makefile rules common to all examples.
include ../../Makefile.base

# Package up all object files, created from the above sources.
libnaomitmpfs.a: ${OBJS}
	$(AR) -rcs -o $@ $^

# Package up all the dependencies and such that libnaomi needs and put them
# into our install directory.
.PHONY: install
install: libnaomitmpfs.a
	mkdir -p ${NAOMI_SH_BASE}/lib
	cp libnaomitmpfs.a ${NAOMI_SH_BASE}/lib

# Include a simple clean target which wipes the build directory
# and kills any library built.
.PHONY: clean
clean:
	rm -rf build
	rm -rf libnaomitmpfs.a
