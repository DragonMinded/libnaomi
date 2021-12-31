In order to minimize the number of patches to newlib, as well as keep newlib and
libnaomi from containing too many circular dependencies, we include some override
includes here that get copied on a "make install". Basically, the base makefiles
ensure that where these get copied is first in the include search order before the
GCC/newlib default includes, so they get picked up first. That means we can safely
compile newlib but provide additional functionality here. Its important that we do
not change any existing types here, only add new types, because the object files
compiled for newlib will use the non-overridden types.
