#ifndef __SRAMFS_H
#define __SRAMFS_H

#ifdef __cplusplus
extern "C" {
#endif

// Attach to SRAM using an internal filesystem that allows for normal POSIX operations such
// as file reading/writing, directory creation, traversal, etc. If you attach to prefix
// "sram", a file at the root of the SRAM FS named "test.txt" would be available at
// "sram://test.txt". Returns 0 on success or a negative number on failure.
int sramfs_init(char *prefix);

// Detach from a previously attached SRAM FS using the prefix given in the init.
void sramfs_free(char *prefix);

// Performs the identical operation to sramfs_init, using the default filesystem prefix
// of "sram".
int sramfs_init_default();

// Detach from the default SRAM FS that was attached to in sramfs_init_default().
void sramfs_free_default();

#ifdef __cplusplus
}
#endif

#endif
