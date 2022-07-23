#ifndef __TMPFS_H
#define __TMPFS_H

#ifdef __cplusplus
extern "C" {
#endif

// The maximum number of registered temp filesystems that we can handle.
#define MAX_TMP_FILESYSTEMS 32

// Create a temporary in-meomry filesystem that allows for normal POSIX operations such
// as file reading/writing, directory creation, traversal, etc. If you attach to prefix
// "tmp", a file at the root of the TMPFS named "test.txt" would be available at
// "tmp://test.txt". Returns 0 on success or a negative number on failure. The size is
// specified in bytes, and is rounded down to the nearest multiple of 256. The location
// can be used to point at a previously-created chunk of memory, or can be left NULL to
// request that the memory be managed for you.
int tmpfs_init(char *prefix, void *location, unsigned int size);

// Detach from a previously attached TMPFS using the prefix given in the init.
void tmpfs_free(char *prefix);

// Performs the identical operation to tmpfs_init, using the default filesystem prefix
// of "tmp". The size is specified in bytes, and is rounded down to the nearest multiple
// of 256. The location can be used to point at a previously-created chunk of memory, or
// can be left NULL to request that the memory be managed for you.
int tmpfs_init_default(void *location, unsigned int size);

// Detach from the default TMPFS that was attached to in tmpfs_init_default().
void tmpfs_free_default();

#ifdef __cplusplus
}
#endif

#endif
