#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#ifdef FEATURE_LITTLEFS
// Unfortunately lfs doesn't seem to be designed to be installed, so we have
// to define this here.
#define LFS_THREADSAFE
#include <lfs.h>
#endif
#include "naomi/system.h"
#include "naomi/thread.h"
#include "naomi/posix.h"
#include "../irqinternal.h"

#ifdef FEATURE_LITTLEFS

// Block size for files.
#define SRAMFS_BLOCK_SIZE 256

// Safe start address for filesystem. The Naomi BIOS messes with the first 0x1F8
// bytes and expects to CRC over them. Most games use a similar storage method.
// We forego that here. It might be worth reversing and documenting more of this
// at some point, but for now this is good enough. For now, it looks like there
// are 8 bytes of something, then an EEPROM CRC at 0x008 and another at 0x100.
// Both protect sections that are 248 bytes long (including the CRC). After that,
// it appears that games use a similar pattern to EEPROMs, where the first 4 bytes
// at 0x1F8 are a CRC over data, then 4 bytes of data length, 4 bytes of that length
// repeated, then 4 bytes of padding. This is repeated for the second section at 0x208,
// followed by length bytes of game data protected by the first CRC, and then length
// bytes of game data protected by the second CRC. We ignore all that since the BIOS
// leaves everything at and past 0x1F8 alone, and just start at 0x200 with a block
// size of 0x100.
#define SRAMFS_START (SRAM_BASE + 0x200)
#define SRAMFS_END (SRAM_BASE + SRAM_SIZE)

// Number of actual blocks in our FS.
#define SRAMFS_BLOCKS ((SRAMFS_END - SRAMFS_START) / SRAMFS_BLOCK_SIZE)

// Our mutex for thread-safe file access.
static mutex_t sram_lock;

// The instance of LFS once we're initialized.
static lfs_t lfs;

int _sram_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size)
{
    uint32_t sram_loc = SRAMFS_START + (block * c->block_size) + off;
    if (sram_loc < SRAMFS_START || (sram_loc + size) > SRAMFS_END)
    {
        _irq_display_invariant("sramfs failure", "tried to read outside of SRAM!");
    }
    memcpy(buffer, (void *)sram_loc, size);
    return 0;
}

int _sram_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size)
{
    uint32_t sram_loc = SRAMFS_START + (block * c->block_size) + off;
    if (sram_loc < SRAMFS_START || (sram_loc + size) > SRAMFS_END)
    {
        _irq_display_invariant("sramfs failure", "tried to write outside of SRAM!");
    }
    memcpy((void *)sram_loc, buffer, size);
    return 0;
}

int _sram_erase(const struct lfs_config *c, lfs_block_t block)
{
    uint32_t sram_loc = SRAMFS_START + (block * c->block_size);
    if (sram_loc < SRAMFS_START || (sram_loc + c->block_size) > SRAMFS_END)
    {
        _irq_display_invariant("sramfs failure", "tried to erase outside of SRAM!");
    }
    memset((void *)sram_loc, 0, c->block_size);
    return 0;
}

int _sram_sync(const struct lfs_config *c)
{
    // We are on a battery-backed SRAM so no sync needed.
    return 0;
}

int _sram_lock_mutex(const struct lfs_config *c)
{
    mutex_lock(&sram_lock);
    return 0;
}

int _sram_unlock_mutex(const struct lfs_config *c)
{
    mutex_unlock(&sram_lock);
    return 0;
}

const struct lfs_config cfg = {
    // block device operations
    .read   = _sram_read,
    .prog   = _sram_prog,
    .erase  = _sram_erase,
    .sync   = _sram_sync,
    .lock   = _sram_lock_mutex,
    .unlock = _sram_unlock_mutex,

    // block device configuration
    .read_size = 1,
    .prog_size = 1,
    .block_size = SRAMFS_BLOCK_SIZE,
    .block_count = SRAMFS_BLOCKS,
    .block_cycles = -1,
    .cache_size = SRAMFS_BLOCK_SIZE,
    .lookahead_size = 16,
};

int lfs_err_to_errno(int lfs_err)
{
    // Not actually an error.
    if (lfs_err == LFS_ERR_OK) { return 0; }

    switch(lfs_err)
    {
        case LFS_ERR_IO:
        case LFS_ERR_CORRUPT:
            return -EIO;
        case LFS_ERR_NOENT:
            return -ENOENT;
        case LFS_ERR_EXIST:
            return -EEXIST;
        case LFS_ERR_NOTDIR:
            return -ENOTDIR;
        case LFS_ERR_ISDIR:
            return -EISDIR;
        case LFS_ERR_NOTEMPTY:
            return -ENOTEMPTY;
        case LFS_ERR_BADF:
            return -EBADF;
        case LFS_ERR_FBIG:
        case LFS_ERR_INVAL:
        case LFS_ERR_NOATTR:
        case LFS_ERR_NAMETOOLONG:
            return -EINVAL;
        case LFS_ERR_NOSPC:
            return -ENOSPC;
        case LFS_ERR_NOMEM:
            return -ENOMEM;
    }

    // Don't know what this is.
    return -EINVAL;
}

void *_sramfs_open(void *fshandle, const char *name, int flags, int mode)
{
    if (flags & O_DIRECTORY)
    {
        // Don't support directory listing through open/read/close.
        return (void *)-ENOTSUP;
    }

    lfs_file_t *file = malloc(sizeof(lfs_file_t));
    if (!file)
    {
        return (void *)-ENOMEM;
    }

    // Map flags from posix to lfs.
    int actual_flags = 0;
    if ((flags & O_RDWR) == O_RDWR)
    {
        actual_flags |= LFS_O_RDWR;
    }
    else
    {
        if ((flags & O_RDONLY) == O_RDONLY) { actual_flags |= LFS_O_RDONLY; }
        if ((flags & O_WRONLY) == O_WRONLY) { actual_flags |= LFS_O_WRONLY; }
    }
    if ((flags & O_CREAT) == O_CREAT) { actual_flags |= LFS_O_CREAT; }
    if ((flags & O_APPEND) == O_APPEND) { actual_flags |= LFS_O_APPEND; }
    if ((flags & O_TRUNC) == O_TRUNC) { actual_flags |= LFS_O_TRUNC; }
    if ((flags & O_EXCL) == O_EXCL) { actual_flags |= LFS_O_EXCL; }

    // Actually attempt to open the file.
    int err = lfs_file_open(fshandle, file, name, actual_flags);
    if (err)
    {
        return (void *)lfs_err_to_errno(err);
    }

    return file;
}

int _sramfs_close(void *fshandle, void *file)
{
    int retval = lfs_err_to_errno(lfs_file_close(fshandle, file));
    free(file);
    return retval;
}

int _sramfs_read(void *fshandle, void *file, void *ptr, int len)
{
    int retval = lfs_file_read(fshandle, file, ptr, len);
    if (retval >= 0)
    {
        return retval;
    }

    return lfs_err_to_errno(retval);
}

int _sramfs_write(void *fshandle, void *file, const void *ptr, int len)
{
    int retval = lfs_file_write(fshandle, file, ptr, len);
    if (retval >= 0)
    {
        return retval;
    }

    return lfs_err_to_errno(retval);
}

int _sramfs_fstat(void *fshandle, void *file, struct stat *st )
{
    // libnaomi only stats open files, but lfs only returns stats on closed files.
    // So we must gather the minimum stats here.
    memset(st, 0, sizeof(struct stat));
    st->st_mode = S_IFREG;
    st->st_nlink = 1;

    lfs_soff_t cur = lfs_file_tell(fshandle, file);
    lfs_file_seek(fshandle, file, 0, LFS_SEEK_END);
    st->st_size = lfs_file_tell(fshandle, file);
    lfs_file_seek(fshandle, file, cur, LFS_SEEK_SET);

    return 0;
}

int _sramfs_lseek(void *fshandle, void *file, _off_t amount, int dir)
{
    int whence;
    switch(dir)
    {
        case SEEK_SET:
            whence = LFS_SEEK_SET;
            break;
        case SEEK_CUR:
            whence = LFS_SEEK_CUR;
            break;
        case SEEK_END:
            whence = LFS_SEEK_END;
            break;
        default:
            return -EINVAL;
    }
    lfs_soff_t off = lfs_file_seek(fshandle, file, amount, whence);
    if (off >= 0)
    {
        return off;
    }

    return lfs_err_to_errno(off);
}

int _sramfs_mkdir(void *fshandle, const char *dir, int flags)
{
    return lfs_err_to_errno(lfs_mkdir(fshandle, dir));
}

int _sramfs_rename( void *fshandle, const char *oldname, const char *newname)
{
    return lfs_err_to_errno(lfs_rename(fshandle, oldname, newname));
}

int _sramfs_unlink( void *fshandle, const char *name)
{
    return lfs_err_to_errno(lfs_remove(fshandle, name));
}

void *_sramfs_opendir(void *fshandle, const char *path)
{
    lfs_dir_t *dir = malloc(sizeof(lfs_dir_t));
    if (!dir)
    {
        return (void *)-ENOMEM;
    }

    int err = lfs_dir_open(fshandle, dir, path);
    if (err)
    {
        return (void *)lfs_err_to_errno(err);
    }

    return dir;
}

int _sramfs_readdir(void *fshandle, void *dir, struct dirent *entry)
{
    // First, try to read the directory entry at all.
    struct lfs_info info;
    int ret = lfs_dir_read(fshandle, dir, &info);
    if (ret < 0)
    {
        // Errored out, return that.
        return lfs_err_to_errno(ret);
    }
    if (ret == 0)
    {
        // End of directory.
        return 0;
    }

    // This was a valid read!
    memset(entry->d_name, 0, NAME_MAX + 1);
    strncpy(entry->d_name, info.name, LFS_NAME_MAX < NAME_MAX ? LFS_NAME_MAX : NAME_MAX);

    switch (info.type)
    {
        case LFS_TYPE_REG:
            entry->d_type = DT_REG;
            break;
        case LFS_TYPE_DIR:
            entry->d_type = DT_DIR;
            break;
        default:
            entry->d_type = DT_UNKNOWN;
            break;
    }

    // Unfortunately lfs doesn't expose inodes.
    entry->d_ino = 0;

    // Read a single entry!
    return 1;
}

int _sramfs_closedir(void *fshandle, void *dir)
{
    int retval = lfs_err_to_errno(lfs_dir_close(fshandle, dir));
    free(dir);
    return retval;
}

static filesystem_t sramfs_hooks = {
    _sramfs_open,
    _sramfs_fstat,
    _sramfs_lseek,
    _sramfs_read,
	_sramfs_write,
    _sramfs_close,
    0,  // We don't support link.
    _sramfs_mkdir,
    _sramfs_rename,
    _sramfs_unlink,
    _sramfs_opendir,
    _sramfs_readdir,
    0,  // lfs seekdir/telldir is weird, so we just don't support it.
    _sramfs_closedir,
};

int sramfs_init(char *prefix)
{
    // Mutex for locking operations on filesystem to be thread-safe.
    mutex_init(&sram_lock);

    // First, try mounting the filesystem
    int err = lfs_mount(&lfs, &cfg);

    // If that failed, reformat to get a fresh SRAM.
    if (err)
    {
        err = lfs_format(&lfs, &cfg);
        if (!err)
        {
            err = lfs_mount(&lfs, &cfg);
        }
    }

    if (!err)
    {
        // Now, work out the prefix we will be using for this filesystem.
        char actual_prefix[32];

        // Copy the prefix over, make sure it fits within the 27 characters
        // of the filesystem prefix.
        strncpy(actual_prefix, prefix, MAX_PREFIX_LEN - 2);
        actual_prefix[MAX_PREFIX_LEN - 2] = 0;

        // Append ":/" to the end of it.
        strcat(actual_prefix, ":/");

        // Attach to posix functions.
        err = attach_filesystem(actual_prefix, &sramfs_hooks, &lfs);
    }

    return err ? -1 : 0;
}

int sramfs_init_default()
{
    return sramfs_init("sram");
}

void sramfs_free(char *prefix)
{
    // Free our hooks.
    char actual_prefix[32];

    // Copy the prefix over, make sure it fits within the 27 characters
    // of the filesystem prefix.
    strncpy(actual_prefix, prefix, MAX_PREFIX_LEN - 2);
    actual_prefix[MAX_PREFIX_LEN - 2] = 0;

    // Append ":/" to the end of it.
    strcat(actual_prefix, ":/");
    detach_filesystem(actual_prefix);

    // Free the filesystem itself.
    lfs_unmount(&lfs);

    // No longer need our mutex.
    mutex_free(&sram_lock);
}

void sramfs_free_default()
{
    sramfs_free("sram");
}

#else
int sramfs_init(char *prefix)
{
    // No support for SRAM FS.
    return -1;
}

int sramfs_init_default()
{
    // No support for SRAM FS.
    return -1;
}

void sramfs_free(char *prefix)
{
    // Empty, no support for SRAM FS.
}

void sramfs_free_default()
{
    // Empty, no support for SRAM FS.
}
#endif
