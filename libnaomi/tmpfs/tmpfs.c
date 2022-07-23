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
#include "naomi/tmpfs/tmpfs.h"
#include "naomi/system.h"
#include "naomi/thread.h"
#include "naomi/posix.h"
#include "../irqinternal.h"

#ifdef FEATURE_LITTLEFS

// Block size for files.
#define TMPFS_BLOCK_SIZE 256

typedef struct {
    // The filesystem we're registered under.
    char prefix[MAX_PREFIX_LEN + 1];

    // Filesystem-specific stuff.
    uint32_t start;
    uint32_t end;
    mutex_t lock;
    lfs_t *lfs;
    struct lfs_config *cfg;
    void *owned;
} tmpfs_context_t;

static tmpfs_context_t filesystems[MAX_TMP_FILESYSTEMS] = { 0 };

int _tmpfs_read_hook(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size)
{
    tmpfs_context_t *context = c->context;
    uint32_t tmpfs_loc = context->start + (block * c->block_size) + off;
    if (tmpfs_loc < context->start || (tmpfs_loc + size) > context->end)
    {
        _irq_display_invariant("tmpfs failure", "tried to read outside of TMPFS!");
    }
    memcpy(buffer, (void *)tmpfs_loc, size);
    return 0;
}

int _tmpfs_prog_hook(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size)
{
    tmpfs_context_t *context = c->context;
    uint32_t tmpfs_loc = context->start + (block * c->block_size) + off;
    if (tmpfs_loc < context->start || (tmpfs_loc + size) > context->end)
    {
        _irq_display_invariant("tmpfs failure", "tried to write outside of TMPFS!");
    }
    memcpy((void *)tmpfs_loc, buffer, size);
    return 0;
}

int _tmpfs_erase_hook(const struct lfs_config *c, lfs_block_t block)
{
    tmpfs_context_t *context = c->context;
    uint32_t tmpfs_loc = context->start + (block * c->block_size);
    if (tmpfs_loc < context->start || (tmpfs_loc + c->block_size) > context->end)
    {
        _irq_display_invariant("tmpfs failure", "tried to erase outside of TMPFS!");
    }
    memset((void *)tmpfs_loc, 0, c->block_size);
    return 0;
}

int _tmpfs_sync_hook(const struct lfs_config *c)
{
    // We are in memory, so there is no sync necessary.
    return 0;
}

int _tmpfs_lock_mutex(const struct lfs_config *c)
{
    tmpfs_context_t *context = c->context;
    mutex_lock(&context->lock);
    return 0;
}

int _tmpfs_unlock_mutex(const struct lfs_config *c)
{
    tmpfs_context_t *context = c->context;
    mutex_unlock(&context->lock);
    return 0;
}

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

void *_tmpfs_open(void *fshandle, const char *name, int flags, int mode)
{
    tmpfs_context_t *config = fshandle;
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
    int err = lfs_file_open(config->lfs, file, name, actual_flags);
    if (err)
    {
        return (void *)lfs_err_to_errno(err);
    }

    return file;
}

int _tmpfs_close(void *fshandle, void *file)
{
    tmpfs_context_t *config = fshandle;
    int retval = lfs_err_to_errno(lfs_file_close(config->lfs, file));
    free(file);
    return retval;
}

int _tmpfs_read(void *fshandle, void *file, void *ptr, int len)
{
    tmpfs_context_t *config = fshandle;
    int retval = lfs_file_read(config->lfs, file, ptr, len);
    if (retval >= 0)
    {
        return retval;
    }

    return lfs_err_to_errno(retval);
}

int _tmpfs_write(void *fshandle, void *file, const void *ptr, int len)
{
    tmpfs_context_t *config = fshandle;
    int retval = lfs_file_write(config->lfs, file, ptr, len);
    if (retval >= 0)
    {
        return retval;
    }

    return lfs_err_to_errno(retval);
}

int _tmpfs_fstat(void *fshandle, void *file, struct stat *st )
{
    // libnaomi only stats open files, but lfs only returns stats on closed files.
    // So we must gather the minimum stats here.
    tmpfs_context_t *config = fshandle;
    memset(st, 0, sizeof(struct stat));
    st->st_mode = S_IFREG;
    st->st_nlink = 1;

    lfs_soff_t cur = lfs_file_tell(config->lfs, file);
    lfs_file_seek(config->lfs, file, 0, LFS_SEEK_END);
    st->st_size = lfs_file_tell(config->lfs, file);
    lfs_file_seek(config->lfs, file, cur, LFS_SEEK_SET);

    return 0;
}

int _tmpfs_lseek(void *fshandle, void *file, _off_t amount, int dir)
{
    tmpfs_context_t *config = fshandle;
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
    lfs_soff_t off = lfs_file_seek(config->lfs, file, amount, whence);
    if (off >= 0)
    {
        return off;
    }

    return lfs_err_to_errno(off);
}

int _tmpfs_mkdir(void *fshandle, const char *dir, int flags)
{
    tmpfs_context_t *config = fshandle;
    return lfs_err_to_errno(lfs_mkdir(config->lfs, dir));
}

int _tmpfs_rename( void *fshandle, const char *oldname, const char *newname)
{
    tmpfs_context_t *config = fshandle;
    return lfs_err_to_errno(lfs_rename(config->lfs, oldname, newname));
}

int _tmpfs_unlink( void *fshandle, const char *name)
{
    tmpfs_context_t *config = fshandle;
    return lfs_err_to_errno(lfs_remove(config->lfs, name));
}

void *_tmpfs_opendir(void *fshandle, const char *path)
{
    tmpfs_context_t *config = fshandle;
    lfs_dir_t *dir = malloc(sizeof(lfs_dir_t));
    if (!dir)
    {
        return (void *)-ENOMEM;
    }

    int err = lfs_dir_open(config->lfs, dir, path);
    if (err)
    {
        return (void *)lfs_err_to_errno(err);
    }

    return dir;
}

int _tmpfs_readdir(void *fshandle, void *dir, struct dirent *entry)
{
    // First, try to read the directory entry at all.
    tmpfs_context_t *config = fshandle;
    struct lfs_info info;
    int ret = lfs_dir_read(config->lfs, dir, &info);
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

int _tmpfs_closedir(void *fshandle, void *dir)
{
    tmpfs_context_t *config = fshandle;
    int retval = lfs_err_to_errno(lfs_dir_close(config->lfs, dir));
    free(dir);
    return retval;
}

static filesystem_t tmpfs_hooks = {
    _tmpfs_open,
    _tmpfs_fstat,
    _tmpfs_lseek,
    _tmpfs_read,
	_tmpfs_write,
    _tmpfs_close,
    0,  // We don't support link.
    _tmpfs_mkdir,
    _tmpfs_rename,
    _tmpfs_unlink,
    _tmpfs_opendir,
    _tmpfs_readdir,
    0,  // lfs seekdir/telldir is weird, so we just don't support it.
    _tmpfs_closedir,
};

int tmpfs_init(char *prefix, void *location, unsigned int size)
{
    // First, round size down to block size.
    size -= size % TMPFS_BLOCK_SIZE;
    if (size <= 0)
    {
        return -1;
    }

    // Now, attempt to allocate space for tracking structures.
    tmpfs_context_t *context = NULL;
    for (int i = 0; i < MAX_TMP_FILESYSTEMS; i++)
    {
        if (filesystems[i].start == 0 && filesystems[i].end == 0)
        {
            context = &filesystems[i];
            break;
        }
    }
    if (context == NULL)
    {
        return -1;
    }

    context->lfs = malloc(sizeof(lfs_t));
    if (context->lfs == 0)
    {
        return -1;
    }
    memset(context->lfs, 0, sizeof(lfs_t));

    struct lfs_config *cfg = malloc(sizeof(struct lfs_config));
    if (cfg == 0)
    {
        free(context->lfs);
        return -1;
    }
    memset(cfg, 0, sizeof(struct lfs_config));

    // Now, see if we need to allocate some space or not.
    context->owned = NULL;
    if (location == NULL)
    {
        location = malloc(size);
        context->owned = location;
    }
    if (location == NULL)
    {
        free(context->lfs);
        free(cfg);
        return -1;
    }

    // block device operations
    cfg->read = _tmpfs_read_hook;
    cfg->prog = _tmpfs_prog_hook;
    cfg->erase = _tmpfs_erase_hook;
    cfg->sync = _tmpfs_sync_hook;
    cfg->lock = _tmpfs_lock_mutex;
    cfg->unlock = _tmpfs_unlock_mutex;

    // block device configuration
    cfg->read_size = 1;
    cfg->prog_size = 1;
    cfg->block_size = TMPFS_BLOCK_SIZE;
    cfg->block_count = size / TMPFS_BLOCK_SIZE;
    cfg->block_cycles = -1;
    cfg->cache_size = TMPFS_BLOCK_SIZE;
    cfg->lookahead_size = 16;

    // Context itself so we can have multiple filesystems
    cfg->context = context;
    context->cfg = cfg;
    context->start = (uint32_t)location;
    context->end = context->start + size;

    // Mutex for locking operations on filesystem to be thread-safe.
    mutex_init(&context->lock);

    // First, try mounting the filesystem
    int err = lfs_mount(context->lfs, cfg);

    // If that failed, reformat to get a fresh filesystem, and try again.
    if (err)
    {
        err = lfs_format(context->lfs, cfg);
        if (!err)
        {
            err = lfs_mount(context->lfs, cfg);
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
        err = attach_filesystem(actual_prefix, &tmpfs_hooks, context);
    }

    if (!err)
    {
        // Config for filesystem itself.
        strcpy(context->prefix, prefix);
    }
    else
    {
        mutex_free(&context->lock);

        if (context->owned)
        {
            free(context->owned);
            context->owned = 0;
        }
        free(context->lfs);
        free(cfg);

        context->start = 0;
        context->end = 0;
        context->lfs = 0;
        context->cfg = 0;
    }

    return err ? -1 : 0;
}

int tmpfs_init_default(void *location, unsigned int size)
{
    return tmpfs_init("tmp", location, size);
}

void tmpfs_free(char *prefix)
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
    for (int i = 0; i < MAX_TMP_FILESYSTEMS; i++)
    {
        if (strcmp(filesystems[i].prefix, prefix) == 0)
        {
            lfs_unmount(filesystems[i].lfs);

            // No longer need our mutex.
            mutex_free(&filesystems[i].lock);

            if (filesystems[i].owned)
            {
                free(filesystems[i].owned);
                filesystems[i].owned = 0;
            }
            free(filesystems[i].lfs);
            free(filesystems[i].cfg);

            // Mark this filesystem as deallocated.
            filesystems[i].start = 0;
            filesystems[i].end = 0;
            filesystems[i].lfs = 0;
            filesystems[i].cfg = 0;
            strcpy(filesystems[i].prefix, "");
        }
    }
}

void tmpfs_free_default()
{
    tmpfs_free("tmp");
}

#else
int tmpfs_init(char *prefix, void *location, unsigned int size)
{
    // No support for TMPFS.
    return -1;
}

int tmpfs_init_default(void *location, unsigned int size)
{
    // No support for TMPFS.
    return -1;
}

void tmpfs_free(char *prefix)
{
    // Empty, no support for TMPFS.
}

void tmpfs_free_default()
{
    // Empty, no support for TMPFS.
}
#endif
