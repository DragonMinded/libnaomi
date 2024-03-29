# Please see the README for setting up a valid build environment.

# The top-level binary that you wish to produce.
all: spritetest.bin

# All of the source files (.c and .s) that you want to compile.
# You can use relative directories here as well. Note that if
# one of these files is not found, make will complain about a
# missing missing `build/naomi.bin' target, so make sure all of
# these files exist.
SRCS += main.c
SRCS += dejavusans.ttf
SRCS += crate.png
SRCS += sonic.png
SRCS += arrowlt.png
SRCS += arrowrt.png
SRCS += diagonal.png

# These are from https://laredgames.itch.io/gems-coins-free which
# I purchased to have some good sprites for this demo.
SRCS += coins.png
SRCS += color_gems.png
SRCS += gray_gem.png

# Pick up base makefile rules common to all examples.
include ../../Makefile.base

# We are using the add-on sprite library.
LIBS += -lnaomisprite

ifeq (${FEATURE_FREETYPE}, 1)
# Libraries that we want to link against.
LIBS += -lfreetype -lbz2 -lz -lpng16
endif

# Specific buildrule for PNG files for this project.
build/%.o: %.png ${IMG2C_FILE}
	@mkdir -p $(dir $@)
	${IMG2C} build/$<.c --mode RGBA4444 $<
	${CC} -c build/$<.c -o $@

# Provide the top-level ROM creation target for this binary.
# See scripts/makerom.py for details about what is customizable.
spritetest.bin: ${MAKEROM_FILE} ${NAOMI_BIN_FILE}
	${MAKEROM} $@ \
		--title "Sprite Library Test" \
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
	rm -rf spritetest.bin
