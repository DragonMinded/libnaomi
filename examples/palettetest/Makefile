# Please see the README for setting up a valid build environment.

# Commands
MKDIR 		:= mkdir

# Output files
BIN 		:= palettetest.bin

# All targets to Make
all: $(BIN)

# All of the source files (.c and .s) that you want to compile.
# You can use relative directories here as well. Note that if
# one of these files is not found, make will complain about a
# missing missing `build/naomi.bin' target, so make sure all of
# these files exist.
SRCS += $(wildcard ./*.c)

SRCS += sample_arrow.png
SRCS += sample_gradient.png 
SRCS += sample_spinner.png
SRCS += pal_a.png
SRCS += pal_b.ase
SRCS += pal_c.gpl
SRCS += pal_d.txt

# Libraries we are linking against
LIBS += -lnaomisprite

# Pick up base makefile rules so we don't have to do it by hand.
include ${NAOMI_BASE}/tools/Makefile.base

# Specific buildrule for PNG files for this project.

# Example of specifying palette build for a PNG
build/pal_a.o: pal_a.png ${PAL2C_FILE}
	@mkdir -p $(dir $@)
	${PAL2C} build/$<.c --mode RGBA8888 $<
	${CC} -c build/$<.c -o $@


build/%.o: %.png ${IMG2C_FILE}
	@mkdir -p $(dir $@)
	${IMG2C} build/$<.c --mode INTENSITY4 $<
	${CC} -c build/$<.c -o $@

build/%.o: %.ase ${PAL2C_FILE}
	@mkdir -p $(dir $@)
	${PAL2C} build/$<.c --mode RGBA8888 $<
	${CC} -c build/$<.c -o $@

build/%.o: %.aco ${PAL2C_FILE}
	@mkdir -p $(dir $@)
	${PAL2C} build/$<.c --mode RGBA8888 $<
	${CC} -c build/$<.c -o $@

build/%.o: %.gpl ${PAL2C_FILE}
	@mkdir -p $(dir $@)
	${PAL2C} build/$<.c --mode RGBA8888 $<
	${CC} -c build/$<.c -o $@

build/%.o: %.pal ${PAL2C_FILE}
	@mkdir -p $(dir $@)
	${PAL2C} build/$<.c --mode RGBA8888 $<
	${CC} -c build/$<.c -o $@

build/pal_d.o: pal_d.txt ${PAL2C_FILE}
	@mkdir -p $(dir $@)
	${PAL2C} build/$<.c --mode RGBA8888 $<
	${CC} -c build/$<.c -o $@

# Provide the top-level ROM creation target for this binary.
# See tools/makerom.py for details about what is customizable.
$(BIN): ${MAKEROM_FILE} ${NAOMI_BIN_FILE} | $(BINDIR)
	${MAKEROM} $@ \
		--title "Palette Test" \
		--publisher "Softdrink117" \
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
	rm -rf $(BIN)
	rm -rf $(BINDIR)
