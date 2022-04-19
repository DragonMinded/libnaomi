# Minimal Build Environment

Minimal Makefile and environment for compiling homebrew for the Naomi system. Assuming
you've followed the install directions in the parent directory, this should compile
to a binary you can netboot or load in an emulator by typing `make`. The base Makefile
comes with several features turned on by default. You get correct compilation and linking
against libnaomi for both C and C++, automatic inclusion of the correct system libraries,
dependency tracking including system library changes and all header files as well as rules
for including several binary file types directly in your source. You also have library and
include paths set up for you properly as well as available tool paths for several handy
tools pre-set.

## Building

Out of the box, you can simply type `make`, assuming that you have followed the isntructions
for setting up libnaomi and have activated the toolchain environment. Subsequent builds with
`make` will perform a recompile only of changed dependencies. Typing `make clean` will wipe
the slate clean and force a full rebuild on the next invocation of `make`. All build
artifacts can be found in the `build/` directory, including the pre-stripped `build.elf`
which can be used with GDB to perform source-level on-target debugging.

There are a few build variables available for you to determine whether certain features are
enabled. This mostly revolves around whether or not some 3rd party libraries have been
built into libnaomi. If freetype is available then `FEATURE_FREETYPE` will be set to 1 in
the makefile and the `FEATURE_FREETYPE` macro will be defined. If zlib is available then
`FEATURE_ZLIB` will be set to 1 in the makefile and the `FEATURE_ZLIB` macro will be defined.

Additionally, the following macros will be defined so that you can use them in code. `SERIAL`
will be set to a 4-character string that matches the serial number built into the ROM header.
`BUILD_DATE` will be set to an integer in the form of YYYYMMDD representing the date that
a particular file was built. `START_ADDR` will be set to the entrypoint address of the ROM
after it was built.

## Customizing

There are several defaults that you can override with Makefile variables. Make sure that you
place the overrides before the `include ${NAOMI_BASE}/tools/Makefile.base` line or they won't
get picked up. The various customizations are documented below.

### SRCS

The `SRCS` variable is how you specify what sources you want to compile. You can give it C,
C++ and assembly source files (`.c`, `.cpp` and `.s` respectively and they will be built
and their header dependencies tracked for you automatically. You can also give it a `.raw`
or a `.ttf` file in order to auto-convert the contents of the file to a C array. Several
examples do this. You can also specify `.png` or `.jpg` images to be converted to sprites
and compiled in as C arrays but do note that there are no automatic build rules to do this.
This is because you must specify what output format you want for each sprite. See the
spritetest example for how to do this.

Note that you can also use wildcards here, such as `$(wildcard *.c)` to automatically pick
up every file without manually specifying.

### LIBS

The `LIBS` variable allows you to specify what additional libraries you want to link against.
Assuming that you've built the 3rd party repository, any library that is there is available
for linking against by specifying it here. See the spritetest example for a deminstration of
how to link against libraries conditionally as well as how to link against libraries no matter
what.

### FLAGS

The `FLAGS` variable is added to every C and C++ compilation for you automatically. This is
a good place to add additional defines and any other miscelaneous flags that you need to pass
to CC or C++ when compiling. Normally you won't need to use this as there are more specialized
flags that you can set below, but it can be useful if you are porting code that expects certain
defines to be present. If you are starting a project from scratch, its recommended to use this
to set `-Werror`. Libnaomi sets `-Wall` for you, so this can be a good way of halting the build
on any warnings.

### OPTIMIZATION_LEVEL

The `OPTIMIZATION_LEVEL` variable is provided as a way to overwrite the default optimization
which is set to `-O3`. Normally you will not want to adjust this as setting the value any lower
can cause noticeable performance decreases. However, if you are debugging a thorny issue and GDB
says that key locals are optimized out, you might want to set this to `-O0` and re-run to capture
the problem.

### CSTD

The `CSTD` variable allows you to overwrite the default C standard. By default, libnaomi and all
built code uses the `gnu11` standard. You might want to overwrite this to `c99` or something
similar depending on the code you are porting and your preferences.

### CPPSTD

The `CPPSTD` variable is identical to the `CSTD` variable, but it only gets passed into C++ when
compiling C++ code. By default, libnaomi and all built code uses the `c++11` standard.

### START_ADDR

The `START_ADDR` variable defines the actual memory location for the entrypoint of your code.
This defaults to `0xc021000` and should not normally be changed. However, under certain circumstances
such as when compiling code that is meant to be hooked into commercial games changing this might
be necessary. Note that changing this will change it everywhere including the macro definitions
available to your code.

### SERIAL

The `SERIAL` variable defines the serial number for your actual game. This is defaulted to `B999`
and must start with a `B` followed by two alphanumeric digits and finally a numeric digit. It must
be 4 digits, no more, no less. This will be used to set the ROM header as well as available as
a macro definition in the code. It also gets used by the EEPROM subsystem to determine when to
erase and set up defaults for the game's EEPROM.

### BARE_METAL

Define the `BARE_METAL` variable to 1 in order to build without libnaomi. This is useful when you
need to build raw executable chunks that can be injected into games or used for overlay sections
in your code. Most likely if you are defining this, you will also want to manually specify the
`START_ADDR` variable as well.
