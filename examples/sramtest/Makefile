# Please see the README for setting up a valid build environment.

# The top-level binary that you wish to produce.
all: sramtest.bin

# All of the source files (.c and .s) that you want to compile.
# You can use relative directories here as well. Note that if
# one of these files is not found, make will complain about a
# missing missing `build/naomi.bin' target, so make sure all of
# these files exist.
SRCS += main.c

# Pick up base makefile rules common to all examples.
include ../../Makefile.base

# We need to link against this either way to use the functionality.
LIBS += -lnaomisramfs

ifeq (${FEATURE_LITTLEFS}, 1)
# We are using this in the naomi SRAM FS library, so link it.
LIBS += -llfs
endif

# Provide the top-level ROM creation target for this binary.
# See scripts/makerom.py for details about what is customizable.
sramtest.bin: ${MAKEROM_FILE} ${NAOMI_BIN_FILE}
	${MAKEROM} $@ \
		--title "SRAM Filesystem Example" \
		--publisher "DragonMinded" \
		--serial "${SERIAL}" \
		--section ${NAOMI_BIN_FILE},${START_ADDR} \
		--entrypoint ${MAIN_ADDR} \
		--main-binary-includes-test-binary \
		--test-entrypoint ${TEST_ADDR}

# Include a simple clean target which wipes the build directory
# and kills any binary built.
.PHONY: clean
clean:
	rm -rf build
	rm -rf sramtest.bin
