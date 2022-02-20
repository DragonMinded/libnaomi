#include <sys/stat.h>
#include <sys/times.h>
#include <sys/types.h>
#include <sys/reent.h>
#include <sys/errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <pthread.h>
#include "naomi/interrupt.h"
#include "naomi/posix.h"
#include "naomi/rtc.h"
#include "naomi/thread.h"
#include "irqstate.h"

/* Actual definition of global errno */
int errno;

/* C++ expected definitions */
void *__dso_handle = NULL;

/* stdio hook mutex */
static mutex_t stdio_mutex;

/* pthread_once destructor mutex */
static mutex_t tls_mutex;

/* pthread_mutex static initializer mutex */
static mutex_t static_mutex;

/* One-time destructor thread initializer */
pthread_once_t destructor_once = PTHREAD_ONCE_INIT;

/* Forward definitions for some hook functions. */
void _fs_init();
void _fs_free();

void _posix_init()
{
    mutex_init(&stdio_mutex);
    mutex_init(&tls_mutex);
    mutex_init(&static_mutex);

    _fs_init();
}

void _posix_free()
{
    _fs_free();

    // We intentionally don't kill the mutex here because we want
    // it to protect until the point where threads are torn down.
    // When that happens, all mutexes in the system will also be freed.
}

void __assert_func(const char * file, int line, const char *func, const char *failedexpr)
{
    _irq_display_invariant(
        "assertion failure",
        "assertion \"%s\" failed: file \"%s\", line %d%s%s\n",
        failedexpr,
        file,
        line,
        func ? ", function: " : "", func ? func : ""
    );
}

// Currently hooked stdio calls.
typedef struct stdio_registered_hooks
{
    stdio_t stdio_hooks;
    struct stdio_registered_hooks *next;
} stdio_registered_hooks_t;

stdio_registered_hooks_t *stdio_hooks = 0;

void * hook_stdio_calls( stdio_t *stdio_calls )
{
    if( stdio_calls == NULL )
    {
        /* Failed to hook, bad input */
        return 0;
    }

    /* Safe to hook */
    stdio_registered_hooks_t *new_hooks = malloc(sizeof(stdio_registered_hooks_t));
    if (new_hooks == 0)
    {
        _irq_display_invariant("memory failure", "could not get memory for stdio hooks!");
    }
    new_hooks->stdio_hooks.stdin_read = stdio_calls->stdin_read;
    new_hooks->stdio_hooks.stdout_write = stdio_calls->stdout_write;
    new_hooks->stdio_hooks.stderr_write = stdio_calls->stderr_write;

    /* Make sure another thread doesn't try to access our structure
     * while we're doing this. */
    mutex_lock(&stdio_mutex);

    /* Add it to the list. */
    new_hooks->next = stdio_hooks;
    stdio_hooks = new_hooks;

    /* Safe to use again. */
    mutex_unlock(&stdio_mutex);

    /* Success */
    return (void *)new_hooks;
}

int unhook_stdio_calls( void *prevhook )
{
    int retval = -1;

    mutex_lock(&stdio_mutex);
    if (prevhook != NULL)
    {
        if (stdio_hooks == prevhook)
        {
            stdio_hooks = stdio_hooks->next;
            free(prevhook);
            retval = 0;
        }
        else
        {
            stdio_registered_hooks_t *curhooks = stdio_hooks;
            while(curhooks != 0)
            {
                if (curhooks->next == prevhook)
                {
                    curhooks->next = curhooks->next->next;
                    free(prevhook);
                    retval = 0;
                    break;
                }

                /* Didn't find it, try the next one. */
                curhooks = curhooks->next;
            }
        }
    }

    /* Return 0 if we succeeded, -1 if we couldn't find the hooks. */
    mutex_unlock(&stdio_mutex);
    return retval;
}

#define FS_PREFIX_LEN 28

typedef struct
{
    /* Pointer to the filesystem callbacks for this filesystem. */
    filesystem_t *fs;
    /* Opaque pointer of data that is passed to us from attach_filesystem and
     * we pass back to the filesystem hooks on every call. */
    void *fshandle;
    /* Filesystem prefix, such as 'rom:/' or 'mem:/' that this filesystem is
     * found under when using standard library file routines. */
    char prefix[FS_PREFIX_LEN];
} fs_mapping_t;

typedef struct
{
    /* Index into the filesystem master mapping to get a fs_mapping_t. */
    int fs_mapping;
    /* The handle returned from the filesystem code's open() function which will
     * be passed to all other function calls. */
    void *handle;
    /* The handle as returned to newlib which will be given to all userspace
     * code calling standard file routines. */
    int fileno;
    /* How many copies of ourselves exist. */
    int copies;
} fs_handle_t;

static fs_mapping_t filesystems[MAX_FILESYSTEMS];
static fs_handle_t handles[MAX_OPEN_FILES];

void _fs_init()
{
    uint32_t old_irq = irq_disable();
    memset(filesystems, 0, sizeof(fs_mapping_t) * MAX_FILESYSTEMS);
    memset(handles, 0, sizeof(fs_handle_t) * MAX_OPEN_FILES);
    irq_restore(old_irq);
}

void _fs_free()
{
    uint32_t old_irq = irq_disable();

    // Go through and close all open file handles for all filesystems.
    for (int j = 0; j < MAX_OPEN_FILES; j++)
    {
        if (handles[j].fileno > 0 && handles[j].handle != 0)
        {
            filesystems[handles[j].fs_mapping].fs->close(filesystems[handles[j].fs_mapping].fshandle, handles[j].handle);
        }
    }

    memset(filesystems, 0, sizeof(fs_mapping_t) * MAX_FILESYSTEMS);
    memset(handles, 0, sizeof(fs_handle_t) * MAX_OPEN_FILES);
    irq_restore(old_irq);
}

int attach_filesystem(const char * const prefix, filesystem_t *filesystem, void *fshandle)
{
    /* Sanity checking */
    if (!prefix || !filesystem)
    {
        return -1;
    }

    /* Make sure prefix is valid */
    int len = strlen(prefix);
    if (len < 3 || len >= FS_PREFIX_LEN || prefix[len - 1] != '/' || prefix[len - 2] != ':')
    {
        return -1;
    }

    /* Make sure the prefix doesn't match one thats already inserted */
    for (int i = 0; i < MAX_FILESYSTEMS; i++)
    {
        if (filesystems[i].prefix[0] != 0 && strcmp(filesystems[i].prefix, prefix) == 0)
        {
            /* Filesystem has already been inserted */
            return -2;
        }
    }

    /* Find an open filesystem entry */
    for (int i = 0; i < MAX_FILESYSTEMS; i++ )
    {
        if (filesystems[i].prefix[0] == 0)
        {
            /* Attach the prefix, remember the pointers to the fs functions. */
            strcpy(filesystems[i].prefix, prefix);
            filesystems[i].fs = filesystem;
            filesystems[i].fshandle = fshandle;
            return 0;
        }
    }

    /* No more filesystem handles available */
    return -3;
}

int detach_filesystem( const char * const prefix )
{
    /* Sanity checking */
    if (prefix == 0)
    {
        return -1;
    }

    for (int i = 0; i < MAX_FILESYSTEMS; i++)
    {
        if (filesystems[i].prefix[0] != 0 && strcmp(filesystems[i].prefix, prefix) == 0)
        {
            if (filesystems[i].fs->close != 0)
            {
                /* We found the filesystem, now go through and close every open file handle */
                for (int j = 0; j < MAX_OPEN_FILES; j++)
                {
                    if (handles[j].fileno > 0 && handles[j].fs_mapping == i && handles[j].handle != 0)
                    {
                        filesystems[i].fs->close(filesystems[i].fshandle, handles[j].handle);
                    }
                }
            }

            /* Now zero out the filesystem entry so it can't be found. */
            memset(&filesystems[i], 0, sizeof(fs_mapping_t));

            /* All went well */
            return 0;
        }
    }

    /* Couldn't find the filesystem to free */
    return -2;
}

int _fs_next_free_handle()
{
    /* Start past STDIN, STDOUT, STDERR file handles */
    static int handle = 3;

    /* The handle we're about to give back. If there aren't free handles, return -1. */
    int newhandle = -1;

    /* Make sure we don't screw up and give the same file handle to multiple threads. */
    uint32_t old_irq = irq_disable();
    for (unsigned int j = 0; j < MAX_OPEN_FILES; j++)
    {
        unsigned int slot = (handle + j) % MAX_OPEN_FILES;

        if (handles[slot].fileno == 0)
        {
            /* Consume and then return this handle. */
            newhandle = handle + j;
            handle = newhandle + 1;
            break;
        }
    }

    /* Return either the handle we found, or -1 to indicate no more free files. */
    irq_restore(old_irq);
    return newhandle;
}

int dup(int oldfile)
{
    // Make sure to copy everything atomically.
    uint32_t old_irq = irq_disable();
    int newfile = -1;

    if (handles[oldfile % MAX_OPEN_FILES].fileno == oldfile)
    {
        int oldoffset = oldfile % MAX_OPEN_FILES;

        // Duplicate a file handle, returning a new handle.
        newfile = _fs_next_free_handle();
        if (newfile < 0)
        {
            errno = EMFILE;
            newfile = -1;
        }
        else
        {
            // We have both the old file existing, and a new hanle for it.
            int newoffset = newfile % MAX_OPEN_FILES;

            // Set up the new file.
            handles[newoffset].fileno = newfile;
            handles[newoffset].handle = handles[oldoffset].handle;
            handles[newoffset].fs_mapping = handles[oldoffset].fs_mapping;
            handles[newoffset].copies = handles[oldoffset].copies;

            // Set the copies plus 1 to all copies of this file handle.
            handles[oldoffset].copies++;
            for (int j = 0; j < MAX_OPEN_FILES; j++)
            {
                if (handles[j].handle == handles[oldoffset].handle)
                {
                    handles[j].copies++;
                }
            }
        }
    }
    else
    {
        errno = EBADF;
        newfile = -1;
    }

    irq_restore(old_irq);
    return newfile;
}

FILE * popen(const char *command, const char *type)
{
    // Don't support process open.
    errno = ENOTSUP;
    return 0;
}

int pclose(FILE *stream)
{
    // Don't support process close.
    errno = ENOTSUP;
    return -1;
}

int _fs_get_hooks(int fileno, filesystem_t **fs, void **fshandle, void **handle)
{
    if( fileno < 3 )
    {
        return 0;
    }

    int slot = fileno % MAX_OPEN_FILES;
    if (handles[slot].fileno == fileno)
    {
        // Found it!
        *fs = filesystems[handles[slot].fs_mapping].fs;
        *fshandle = filesystems[handles[slot].fs_mapping].fshandle;
        *handle = handles[slot].handle;
        return 1;
    }

    // Couldn't find it.
    return 0;
}

int _fs_get_fs_by_name(const char * const name)
{
    /* Invalid */
    if (name == 0)
    {
        return -1;
    }

    for(int i = 0; i < MAX_FILESYSTEMS; i++)
    {
        if (filesystems[i].prefix[0] != 0 && strncmp(filesystems[i].prefix, name, strlen(filesystems[i].prefix)) == 0)
        {
            /* Found it! */
            return i;
        }
    }

    /* Couldn't find it */
    return -1;

}

char *realpath(const char *restrict path, char *restrict resolved_path)
{
    if (path == 0)
    {
        errno = EINVAL;
        return 0;
    }

    int mapping = _fs_get_fs_by_name(path);
    if (mapping >= 0)
    {
        const char *fullpath = path + strlen(filesystems[mapping].prefix);

        if (fullpath[0] != '/')
        {
            // Paths MUST be absolute, we do not support chdir()!
            errno = ENOENT;
            return 0;
        }
        // Skip past leading '/'.
        fullpath ++;

        // We need some memory for resolved_path if its not provided.
        int allocated = 0;
        if (resolved_path == 0)
        {
            resolved_path = malloc(PATH_MAX + 1);
            if (resolved_path == 0)
            {
                errno = ENOMEM;
                return 0;
            }
            allocated = 1;
        }

        if (fullpath[0] != 0)
        {
            char **parts = malloc(sizeof(char **));
            if (parts == 0)
            {
                if (allocated)
                {
                    free(resolved_path);
                }
                errno = ENOMEM;
                return 0;
            }
            parts[0] = malloc(PATH_MAX + 1);
            if (parts[0] == 0)
            {
                if (allocated)
                {
                    free(resolved_path);
                }
                free(parts);
                errno = ENOMEM;
                return 0;
            }
            memset(parts[0], 0, PATH_MAX + 1);
            int partscount = 1;
            int partpos = 0;
            int trailing_slash = 0;

            // Separate out into parts.
            while (fullpath[0] != 0)
            {
                if (fullpath[0] == '/')
                {
                    if (fullpath[1] == 0)
                    {
                        // Don't need a new allocation, we're good as-is.
                        trailing_slash = 1;
                        break;
                    }
                    else
                    {
                        // Need to allocate more for parts.
                        partscount++;
                        parts = realloc(parts, sizeof(char **) * partscount);
                        parts[partscount - 1] = malloc(PATH_MAX + 1);
                        if (parts[partscount - 1] == 0)
                        {
                            if (allocated)
                            {
                                free(resolved_path);
                            }
                            for (int i = 0; i < partscount - 1; i++)
                            {
                                free(parts[i]);
                            }
                            free(parts);
                            errno = ENOMEM;
                            return 0;
                        }
                        memset(parts[partscount - 1], 0, PATH_MAX + 1);
                        partpos = 0;
                    }

                    // Skip past this character.
                    fullpath++;
                }
                else
                {
                    parts[partscount - 1][partpos] = fullpath[0];
                    partpos++;
                    fullpath++;
                }
            }

            // At this point we have a string of path parts. Now we need to rejoin
            // them all canonicalized. First, take care of . and .. in the path.
            char **newparts = malloc(sizeof(char **) * partscount);
            if (newparts == 0)
            {
                if (allocated)
                {
                    free(resolved_path);
                }
                for (int i = 0; i < partscount; i++)
                {
                    free(parts[i]);
                }
                free(parts);
                errno = ENOMEM;
                return 0;
            }
            int newpartscount = 0;
            for (int i = 0; i < partscount; i++)
            {
                if (parts[i][0] == 0 || strcmp(parts[i], ".") == 0)
                {
                    // Ignore it, its just pointing at the current directory.
                }
                else if (strcmp(parts[i], "..") == 0)
                {
                    // Pop one directory.
                    if (newpartscount > 0)
                    {
                        newpartscount--;
                    }
                }
                else
                {
                    // Push one directory.
                    newparts[newpartscount] = parts[i];
                    newpartscount++;
                }
            }

            // Now, we must go through and make sure each part of the canonical path
            // is actually a directory.
            int all_okay = 1;
            strcpy(resolved_path, filesystems[mapping].prefix);
            strcat(resolved_path, "/");

            for (int i = 0; i < newpartscount; i++)
            {
                // First, concatenate it onto the path.
                if (resolved_path[strlen(resolved_path) - 1] != '/')
                {
                    strcat(resolved_path, "/");
                }
                strcat(resolved_path, newparts[i]);

                // Second, make sure it is a directory. It can only be a file if it
                // is the last entry in the path.
                struct stat st;
                if (stat(resolved_path, &st) != 0)
                {
                    // We leave the errno alone so it can be returned.
                    all_okay = 0;
                    break;
                }

                if ((st.st_mode & S_IFDIR) != 0)
                {
                    // Its a directory!
                    if (i == (newpartscount - 1))
                    {
                        // Need to append a final '/'.
                        strcat(resolved_path, "/");
                    }
                }
                else if ((st.st_mode & S_IFREG) != 0)
                {
                    // It can only be a file if it is the last part.
                    if (i != (newpartscount - 1))
                    {
                        errno = ENOTDIR;
                        all_okay = 0;
                        break;
                    }
                    else if(trailing_slash)
                    {
                        errno = ENOTDIR;
                        all_okay = 0;
                        break;
                    }
                }
                else
                {
                    // Unclear what this is, not valid.
                    errno = ENOTDIR;
                    all_okay = 0;
                    break;
                }
            }

            // Now, free up memory.
            for (int i = 0; i < partscount; i++)
            {
                free(parts[i]);
            }
            free(parts);
            free(newparts);

            // Finally, return it if it was okay.
            if (all_okay)
            {
                return resolved_path;
            }
            else
            {
                if (allocated)
                {
                    free(resolved_path);
                }

                return 0;
            }
        }
        else
        {
            // Path is already normalized root path.
            strcpy(resolved_path, path);
            return resolved_path;
        }
    }
    else
    {
        errno = ENOENT;
        return 0;
    }
}

_ssize_t _read_r(struct _reent *reent, int file, void *ptr, size_t len)
{
    if( file == 0 )
    {
        /* If we don't get a valid hook, then this wasn't supported. */
        int retval = -ENOTSUP;

        /* Only read from the first valid hooks. */
        mutex_lock(&stdio_mutex);
        stdio_registered_hooks_t *curhook = stdio_hooks;
        while (curhook != 0)
        {
            if (curhook->stdio_hooks.stdin_read)
            {
                retval = curhook->stdio_hooks.stdin_read( ptr, len );
                break;
            }

            curhook = curhook->next;
        }

        mutex_unlock(&stdio_mutex);

        if (retval < 0)
        {
            reent->_errno = -retval;
            return -1;
        }
        else
        {
            return retval;
        }
    }
    else if( file == 1 || file == 2 )
    {
        /* Can't read from output buffers */
        reent->_errno = EBADF;
        return -1;
    }
    else
    {
        /* Attempt to use filesystem hooks to perform read */
        filesystem_t *fs = 0;
        void *fshandle = 0;
        void *handle = 0;
        if (_fs_get_hooks(file, &fs, &fshandle, &handle))
        {
            if (fs->read == 0)
            {
                /* Filesystem doesn't support read */
                reent->_errno = ENOTSUP;
                return -1;
            }

            int retval = fs->read(fshandle, handle, ptr, len);
            if (retval < 0)
            {
                reent->_errno = -retval;
                return -1;
            }
            else
            {
                return retval;
            }
        }

        /* There is no filesystem backing this file. */
        reent->_errno = ENOTSUP;
        return -1;
    }
}

_off_t _lseek_r(struct _reent *reent, int file, _off_t amount, int dir)
{
    /* Attempt to use filesystem hooks to perform lseek */
    filesystem_t *fs = 0;
    void *fshandle = 0;
    void *handle = 0;
    if (_fs_get_hooks(file, &fs, &fshandle, &handle))
    {
        if (fs->lseek == 0)
        {
            /* Filesystem doesn't support lseek */
            reent->_errno = ENOTSUP;
            return -1;
        }

        int retval = fs->lseek(fshandle, handle, amount, dir);
        if (retval < 0)
        {
            reent->_errno = -retval;
            return -1;
        }
        else
        {
            return retval;
        }
    }

    /* There is no filesystem backing this file. */
    reent->_errno = ENOTSUP;
    return -1;
}

_ssize_t _write_r(struct _reent *reent, int file, const void * ptr, size_t len)
{
    if( file == 0 )
    {
        /* Can't write to input buffers */
        reent->_errno = EBADF;
        return -1;
    }
    else if( file == 1 )
    {
        /* If we don't get a valid hook, then this wasn't supported. */
        int retval = -ENOTSUP;

        /* Write to every single valid hook. Ignore returns. */
        mutex_lock(&stdio_mutex);
        stdio_registered_hooks_t *curhook = stdio_hooks;
        while (curhook != 0)
        {
            if (curhook->stdio_hooks.stdout_write)
            {
                curhook->stdio_hooks.stdout_write( ptr, len );
                retval = len;
            }

            curhook = curhook->next;
        }
        mutex_unlock(&stdio_mutex);

        if (retval < 0)
        {
            reent->_errno = -retval;
            return -1;
        }
        else
        {
            return retval;
        }
    }
    else if( file == 2 )
    {
        /* If we don't get a valid hook, then this wasn't supported. */
        int retval = -ENOTSUP;

        /* Write to every single valid hook. Ignore returns. */
        mutex_lock(&stdio_mutex);
        stdio_registered_hooks_t *curhook = stdio_hooks;
        while (curhook != 0)
        {
            if (curhook->stdio_hooks.stderr_write)
            {
                curhook->stdio_hooks.stderr_write( ptr, len );
                retval = len;
            }

            curhook = curhook->next;
        }
        mutex_unlock(&stdio_mutex);

        if (retval < 0)
        {
            reent->_errno = -retval;
            return -1;
        }
        else
        {
            return retval;
        }
    }
    else
    {
        /* Attempt to use filesystem hooks to perform write */
        filesystem_t *fs = 0;
        void *fshandle = 0;
        void *handle = 0;
        if (_fs_get_hooks(file, &fs, &fshandle, &handle))
        {
            if (fs->write == 0)
            {
                /* Filesystem doesn't support write */
                reent->_errno = ENOTSUP;
                return -1;
            }

            int retval = fs->write(fshandle, handle, ptr, len);
            if (retval < 0)
            {
                reent->_errno = -retval;
                return -1;
            }
            else
            {
                return retval;
            }
        }

        /* There is no filesystem backing this file. */
        reent->_errno = ENOTSUP;
        return -1;
    }
}

int _close_r(struct _reent *reent, int file)
{
    /* Attempt to use filesystem hooks to perform close */
    filesystem_t *fs = 0;
    void *fshandle = 0;
    void *handle = 0;
    if (_fs_get_hooks(file, &fs, &fshandle, &handle))
    {
        /* First, figure out if we need to close this handle at all, or if
         * we have some duplicates hanging around. */
        int copies = handles[file % MAX_OPEN_FILES].copies;

        int retval;
        if (fs->close == 0)
        {
            /* Filesystem doesn't support close */
            retval = -ENOTSUP;
        }
        else
        {
            if (copies == 1)
            {
                /* Perform the close action. */
                retval = fs->close(fshandle, handle);
            }
            else
            {
                /* Don't actually close the file, we have more than one handle around. */
                retval = 0;
            }
        }

        /* Finally, before we return, unregister this handle. */
        for( int i = 0; i < MAX_OPEN_FILES; i++)
        {
            if (handles[i].handle == handle)
            {
                handles[i].copies --;
                if (handles[i].copies == 0)
                {
                    memset(&handles[i], 0, sizeof(fs_handle_t));
                }
            }
            if (handles[i].fileno == file)
            {
                memset(&handles[i], 0, sizeof(fs_handle_t));
            }
        }

        if (retval < 0)
        {
            reent->_errno = -retval;
            return -1;
        }
        else
        {
            return retval;
        }
    }

    /* There is no filesystem backing this file. */
    reent->_errno = ENOTSUP;
    return -1;
}

int _link_r(struct _reent *reent, const char *old, const char *new)
{
    /* Attempt to use filesystem hooks to perform link */
    int oldfs = _fs_get_fs_by_name(old);
    int newfs = _fs_get_fs_by_name(old);

    if (oldfs >= 0 && newfs >= 0)
    {
        /* Make sure both of them are of the same filesystem. */
        if (oldfs != newfs)
        {
            /* We can't link across multiple filesytems. What are we, linux? */
            reent->_errno = ENOTSUP;
            return -1;
        }

        filesystem_t *fs = filesystems[oldfs].fs;
        if (fs->link == 0)
        {
            /* Filesystem doesn't support link */
            reent->_errno = ENOTSUP;
            return -1;
        }

        int retval = fs->link(filesystems[oldfs].fshandle, old + strlen(filesystems[oldfs].prefix), new + strlen(filesystems[newfs].prefix));
        if (retval < 0)
        {
            reent->_errno = -retval;
            return -1;
        }
        else
        {
            return retval;
        }
    }

    /* There is no filesystem backing this file. */
    reent->_errno = ENOTSUP;
    return -1;
}

int _rename_r (struct _reent *reent, const char *old, const char *new)
{
    /* Attempt to use filesystem hooks to perform rename */
    int oldfs = _fs_get_fs_by_name(old);
    int newfs = _fs_get_fs_by_name(old);

    if (oldfs >= 0 && newfs >= 0)
    {
        /* Make sure both of them are of the same filesystem. */
        if (oldfs != newfs)
        {
            /* We can't rename across multiple filesytems. What are we, linux? */
            reent->_errno = ENOTSUP;
            return -1;
        }

        filesystem_t *fs = filesystems[oldfs].fs;
        if (fs->rename == 0)
        {
            /* Filesystem doesn't support rename */
            reent->_errno = ENOTSUP;
            return -1;
        }

        int retval = fs->rename(filesystems[oldfs].fshandle, old + strlen(filesystems[oldfs].prefix), new + strlen(filesystems[newfs].prefix));
        if (retval < 0)
        {
            reent->_errno = -retval;
            return -1;
        }
        else
        {
            return retval;
        }
    }

    /* There is no filesystem backing this file. */
    reent->_errno = ENOTSUP;
    return -1;
}

void *_sbrk_impl(struct _reent *reent, ptrdiff_t incr)
{
    extern char end;  /* Defined by the linker in naomi.ld */
    static char *heap_end;
    char *prev_heap_end;

    if(heap_end == 0)
    {
        heap_end = &end;
    }
    prev_heap_end = heap_end;

    // This really should be checking for the end of stack, but
    // that only really works in the main thread and that only really
    // makes sense if the stack will never grow larger than after
    // this check. So just use the top of memory.
    if(heap_end + incr > (char *)0x0E000000)
    {
        reent->_errno = ENOMEM;
        return (void *)-1;
    }
    heap_end += incr;
    return prev_heap_end;
}

void *_sbrk_r(struct _reent *reent, ptrdiff_t incr)
{
    uint32_t old_interrupts = irq_disable();
    void *ptr = _sbrk_impl(reent, incr);
    irq_restore(old_interrupts);
    return ptr;
}

int _fstat_r(struct _reent *reent, int file, struct stat *st)
{
    /* Attempt to use filesystem hooks to perform fstat */
    filesystem_t *fs = 0;
    void *fshandle = 0;
    void *handle = 0;
    if (_fs_get_hooks(file, &fs, &fshandle, &handle))
    {
        if (fs->fstat == 0)
        {
            /* Filesystem doesn't support fstat */
            reent->_errno = ENOTSUP;
            return -1;
        }

        int retval = fs->fstat(fshandle, handle, st);
        if (retval < 0)
        {
            reent->_errno = -retval;
            return -1;
        }
        else
        {
            return retval;
        }
    }

    /* There is no filesystem backing this file. */
    reent->_errno = ENOTSUP;
    return -1;
}

int _mkdir_r(struct _reent *reent, const char *path, int flags)
{
    /* Attempt to use filesystem hooks to perform mkdir */
    int mapping = _fs_get_fs_by_name(path);
    if (mapping >= 0)
    {
        filesystem_t *fs = filesystems[mapping].fs;
        if (fs->mkdir == 0)
        {
            /* Filesystem doesn't support mkdir */
            reent->_errno = ENOTSUP;
            return -1;
        }

        int retval = fs->mkdir(filesystems[mapping].fshandle, path + strlen(filesystems[mapping].prefix), flags);
        if (retval < 0)
        {
            reent->_errno = -retval;
            return -1;
        }
        else
        {
            return retval;
        }
    }

    /* There is no filesystem backing this file. */
    reent->_errno = ENOTSUP;
    return -1;
}

int _open_r(struct _reent *reent, const char *path, int flags, int mode)
{
    int mapping = _fs_get_fs_by_name(path);
    filesystem_t *fs = 0;

    if (mapping >= 0)
    {
        fs = filesystems[mapping].fs;
    }
    else
    {
        /* There is no fileystem backing this path. */
        reent->_errno = ENOTSUP;
        return -1;
    }

    if (fs->open == 0)
    {
        /* Filesystem doesn't support open */
        reent->_errno = ENOTSUP;
        return -1;
    }

    /* Do we have room for a new file? */
    int newhandle = _fs_next_free_handle();
    if (newhandle < 0)
    {
        /* No file handles available */
        reent->_errno = ENFILE;
        return -1;
    }
    else
    {
        /* Yes, we have room, try the open */
        int slot = newhandle % MAX_OPEN_FILES;
        void *ptr = fs->open(filesystems[mapping].fshandle, path + strlen(filesystems[mapping].prefix), flags, mode);
        int errnoptr = (int)ptr;

        if (errnoptr > 0)
        {
            /* Create new internal handle */
            handles[slot].fileno = newhandle;
            handles[slot].handle = ptr;
            handles[slot].fs_mapping = mapping;
            handles[slot].copies = 1;

            /* Return our own handle */
            return handles[slot].fileno;
        }
        else
        {
            /* Couldn't open for some reason */
            if (errnoptr == 0)
            {
                reent->_errno = ENOENT;
            }
            else
            {
                reent->_errno = -errnoptr;
            }
            return -1;
        }
    }
}

int _unlink_r(struct _reent *reent, const char *path)
{
    /* Attempt to use filesystem hooks to perform unlink */
    int mapping = _fs_get_fs_by_name(path);
    if (mapping >= 0)
    {
        filesystem_t *fs = filesystems[mapping].fs;
        if (fs->unlink == 0)
        {
            /* Filesystem doesn't support unlink */
            reent->_errno = ENOTSUP;
            return -1;
        }

        int retval = fs->unlink(filesystems[mapping].fshandle, path + strlen(filesystems[mapping].prefix));
        if (retval < 0)
        {
            reent->_errno = -retval;
            return -1;
        }
        else
        {
            return retval;
        }
    }

    /* There is no filesystem backing this file. */
    reent->_errno = ENOTSUP;
    return -1;
}

int _isatty_r(struct _reent *reent, int fd)
{
    if (fd == 0 || fd == 1 || fd == 2)
    {
        return 1;
    }
    else
    {
        reent->_errno = ENOTTY;
        return 0;
    }
}

int _kill_r(struct _reent *reent, int n, int m)
{
    // We have threads but no processes, so let's not pretend with half support.
    reent->_errno = ENOTSUP;
    return -1;
}

int _getpid_r(struct _reent *reent)
{
    // We have threads but no processes, so let's not pretend with half support.
    reent->_errno = ENOTSUP;
    return -1;
}

int _stat_r(struct _reent *reent, const char *path, struct stat *st)
{
    /* Attempt to use filesystem hooks to perform stat */
    int mapping = _fs_get_fs_by_name(path);
    if (mapping >= 0)
    {
        filesystem_t *fs = filesystems[mapping].fs;
        if (fs->open == 0 || fs->close == 0 || fs->fstat == 0)
        {
            /* Filesystem doesn't support stat by way of missing utility functions */
            reent->_errno = ENOTSUP;
            return -1;
        }

        /* Open the file, grab the stat, close it */
        void *handle = fs->open(filesystems[mapping].fshandle, path + strlen(filesystems[mapping].prefix), 0, 0666);
        int handleint = (int)handle;

        if (handleint > 0)
        {
            int retval = fs->fstat(filesystems[mapping].fshandle, handle, st);
            fs->close(filesystems[mapping].fshandle, handle);

            /* Return what stat gave us */
            if (retval < 0)
            {
                reent->_errno = -retval;
                return -1;
            }
            else
            {
                return retval;
            }
        }
        else
        {
            if (-handleint == EISDIR)
            {
                /* This is actually a directory, not a file. */
                memset(st, 0, sizeof(struct stat));
                st->st_mode = S_IFDIR;
                st->st_nlink = 1;
                return 0;
            }
            else
            {
                reent->_errno = -handleint;
                return -1;
            }
        }
    }

    /* There is no filesystem backing this file. */
    reent->_errno = ENOTSUP;
    return -1;
}

int _fork_r(struct _reent *reent)
{
    // We have threads but no processes, so let's not pretend with half support.
    reent->_errno = ENOTSUP;
    return -1;
}

int _wait_r(struct _reent *reent, int *statusp)
{
    // We have threads but no processes, so let's not pretend with half support.
    reent->_errno = ENOTSUP;
    return -1;
}

int _execve_r(struct _reent *reent, const char *path, char *const argv[], char *const envp[])
{
    // We have threads but no processes, so let's not pretend with half support.
    reent->_errno = ENOTSUP;
    return -1;
}

_CLOCK_T_ _times_r(struct _reent *reent, struct tms *tm)
{
    // We have threads but no processes, so let's not pretend with half support.
    reent->_errno = ENOTSUP;
    return -1;
}

// Amount of seconds in twenty years not spanning over a century rollover.
// We use this because RTC epoch on Naomi is 1/1/1950 instead of 1/1/1970
// like unix and C standard library expects.
#define TWENTY_YEARS ((20 * 365LU + 5) * 86400)

int _gettimeofday_r(struct _reent *reent, struct timeval *tv, void *tz)
{
    tv->tv_sec = rtc_get() - TWENTY_YEARS;
    tv->tv_usec = 0;
    return 0;
}

typedef struct
{
    void *owner;
    int depth;
    uint32_t old_irq;
} recursive_newlib_lock_t;

recursive_newlib_lock_t newlib_lock = { 0, 0 };

void __malloc_lock (struct _reent *reent)
{
    uint32_t old_irq = irq_disable();

    if (newlib_lock.owner == reent)
    {
        // Increase our depth.
        newlib_lock.depth++;

        // No need to unlock interrupts here, we've already disabled them in the
        // first lock.
        return;
    }
    if (newlib_lock.owner != 0)
    {
        _irq_display_invariant("malloc locking failure", "malloc lock owned by another malloc call during lock!");
    }

    // Lock ourselves, remembering our old IRQ.
    newlib_lock.owner = reent;
    newlib_lock.depth = 1;
    newlib_lock.old_irq = old_irq;
}

void __malloc_unlock (struct _reent *reent)
{
    // Just in case, but we shouldn't have to worry about IRQs being enabled
    // if newlib is coded correctly.
    uint32_t old_irq = irq_disable();

    if (newlib_lock.owner != reent)
    {
        _irq_display_invariant("malloc locking failure", "malloc lock owned by another malloc call during unlock!");
    }

    newlib_lock.depth --;
    if (newlib_lock.depth == 0)
    {
        // Time to unlock here!
        newlib_lock.owner = 0;
        irq_restore(newlib_lock.old_irq);
    }
    else
    {
        // Technically this should do nothing, but at least it is symmetrical.
        irq_restore(old_irq);
    }
}

DIR *opendir(const char *name)
{
    int mapping = _fs_get_fs_by_name(name);
    if (mapping >= 0)
    {
        filesystem_t *fs = filesystems[mapping].fs;
        if (fs->opendir == 0)
        {
            /* Filesystem doesn't support opendir */
            errno = ENOTSUP;
            return 0;
        }

        void *handle = fs->opendir(filesystems[mapping].fshandle, name + strlen(filesystems[mapping].prefix));
        int errnohandle = (int)handle;

        if (errnohandle > 0)
        {
            /* Create new DIR handle */
            DIR *dir = malloc(sizeof(DIR));
            if (dir == 0)
            {
                if (fs->closedir)
                {
                    fs->closedir(filesystems[mapping].fshandle, handle);
                }
                errno = ENOMEM;
                return 0;
            }
            dir->handle = handle;
            dir->fs = mapping;
            dir->ent = malloc(sizeof(struct dirent));
            if (dir->ent == 0)
            {
                if (fs->closedir)
                {
                    fs->closedir(filesystems[mapping].fshandle, handle);
                }
                free(dir);
                errno = ENOMEM;
                return 0;
            }
            else
            {
                return dir;
            }
        }
        else
        {
            /* Couldn't open for some reason */
            if (errnohandle == 0)
            {
                errno = ENOENT;
            }
            else
            {
                errno = -errnohandle;
            }
            return 0;
        }
    }

    /* We don't have a filesystem mapping for this file. */
    errno = ENOTSUP;
    return 0;
}

struct dirent *readdir(DIR *dirp)
{
    if (dirp == 0)
    {
        errno = EINVAL;
        return 0;
    }
    else
    {
        if (dirp->fs >= 0)
        {
            filesystem_t *fs = filesystems[dirp->fs].fs;
            if (fs->readdir == 0)
            {
                /* Filesystem doesn't support readdir */
                errno = ENOTSUP;
                return 0;
            }

            int retval = fs->readdir(filesystems[dirp->fs].fshandle, dirp->handle, dirp->ent);
            if (retval < 0)
            {
                errno = -retval;
                return 0;
            }
            else if(retval > 0)
            {
                return dirp->ent;
            }
            else
            {
                return 0;
            }
        }
        else
        {
            /* Somehow gave us a bogus DIR structure. */
            errno = ENOTSUP;
            return 0;
        }
    }
}

void seekdir(DIR *dirp, long loc)
{
    if (dirp == 0)
    {
        return;
    }
    else
    {
        if (dirp->fs >= 0)
        {
            filesystem_t *fs = filesystems[dirp->fs].fs;
            if (fs->seekdir == 0)
            {
                /* Filesystem doesn't support seekdir/telldir */
                return;
            }

            fs->seekdir(filesystems[dirp->fs].fshandle, dirp->handle, loc);
        }
    }
}

long telldir(DIR *dirp)
{
    if (dirp == 0)
    {
        errno = EINVAL;
        return -1;
    }
    else
    {
        if (dirp->fs >= 0)
        {
            filesystem_t *fs = filesystems[dirp->fs].fs;
            if (fs->seekdir == 0)
            {
                /* Filesystem doesn't support seekdir/telldir */
                errno = ENOTSUP;
                return -1;
            }

            int retval = fs->seekdir(filesystems[dirp->fs].fshandle, dirp->handle, -1);
            if (retval < 0)
            {
                errno = -retval;
                return -1;
            }
            else
            {
                return retval;
            }
        }
        else
        {
            /* Somehow gave us a bogus DIR structure. */
            errno = ENOTSUP;
            return -1;
        }
    }
}

int closedir(DIR *dirp)
{
    if (dirp == 0)
    {
        errno = EINVAL;
        return -1;
    }
    else
    {
        if (dirp->fs >= 0)
        {
            filesystem_t *fs = filesystems[dirp->fs].fs;
            if (fs->closedir == 0)
            {
                /* Filesystem doesn't support closedir */
                errno = ENOTSUP;
                return -1;
            }

            int retval = fs->closedir(filesystems[dirp->fs].fshandle, dirp->handle);
            free(dirp->ent);
            free(dirp);

            if (retval < 0)
            {
                errno = -retval;
                return -1;
            }
            else
            {
                return retval;
            }
        }
        else
        {
            /* Somehow gave us a bogus DIR structure. */
            errno = ENOTSUP;
            return -1;
        }
    }
}

int pthread_attr_init(pthread_attr_t *attr)
{
    memset(attr, 0, sizeof(pthread_attr_t));
    attr->is_initialized = 1;
    attr->stacksize = THREAD_STACK_SIZE;
    attr->detachstate = PTHREAD_CREATE_JOINABLE;
    attr->contentionscope = PTHREAD_SCOPE_SYSTEM;
    return 0;
}

int pthread_attr_destroy(pthread_attr_t *attr)
{
    attr->is_initialized = 0;
    return 0;
}

int pthread_attr_setstack(pthread_attr_t *attr, void *stackaddr, size_t stacksize)
{
    // We don't support changing the stack address.
    return EINVAL;
}

int pthread_attr_getstack(const pthread_attr_t *attr, void **stackaddr, size_t *stacksize)
{
    if (stackaddr)
    {
        *stackaddr = attr->stackaddr;
    }
    if (stacksize)
    {
        *stacksize = attr->stacksize;
    }
    return 0;
}

int pthread_attr_getstacksize(const pthread_attr_t *attr, size_t *stacksize)
{
    if (stacksize)
    {
        *stacksize = attr->stacksize;
    }
    return 0;
}

int pthread_attr_setstacksize(pthread_attr_t *attr, size_t stacksize)
{
    if (stacksize != THREAD_STACK_SIZE)
    {
        // Don't support changing stack size!
        return EINVAL;
    }
    attr->stacksize = stacksize;
    return 0;
}

int pthread_attr_getstackaddr(const pthread_attr_t *attr, void **stackaddr)
{
    if (stackaddr)
    {
        *stackaddr = attr->stackaddr;
    }
    return 0;
}

int pthread_attr_setstackaddr(pthread_attr_t *attr, void *stackaddr)
{
    if (stackaddr != 0)
    {
        // Don't support changing stack address!
        return EINVAL;
    }
    attr->stackaddr = stackaddr;
    return 0;
}

int pthread_attr_getdetachstate(const pthread_attr_t *attr, int *detachstate)
{
    if (detachstate)
    {
        *detachstate = attr->detachstate;
    }
    return 0;
}

int pthread_attr_setdetachstate(pthread_attr_t *attr, int detachstate)
{
    attr->detachstate = detachstate;
    return 0;
}

int pthread_attr_getguardsize(const pthread_attr_t *attr, size_t *guardsize)
{
    // Don't have any support for this at all, not even in the datatype.
    return EINVAL;
}

int pthread_attr_setguardsize(pthread_attr_t *attr, size_t guardsize)
{
    // Don't have any support for this at all, not even in the datatype.
    return EINVAL;
}

// Allow our destructor thread to spoof the original thread from the perspective of
// all pthread functions.
static uint32_t overridden_thread_id = 0;
static uint32_t substituted_thread_id = 0;

uint32_t _thread_from_pthread(pthread_t pid)
{
    uint32_t old_irq = irq_disable();
    uint32_t tid = (uint32_t)pid;

    if (overridden_thread_id != 0 && substituted_thread_id != 0 && tid == substituted_thread_id)
    {
        tid = overridden_thread_id;
    }

    irq_restore(old_irq);
    return (pthread_t)tid;
}

pthread_t pthread_self(void)
{
    uint32_t old_irq = irq_disable();

    // Specifically allow one thread ID to be "changed" to another to support
    // having a separate thread for destructors that still allows calling
    // thread-specific functions such as pthread_self() and pthread_key_delete().
    uint32_t id = thread_id();
    if (overridden_thread_id != 0 && substituted_thread_id != 0 && id == overridden_thread_id)
    {
        id = substituted_thread_id;
    }

    irq_restore(old_irq);
    return (pthread_t)id;
}

int pthread_equal(pthread_t t1, pthread_t t2)
{
    return _thread_from_pthread(t1) == _thread_from_pthread(t2);
}

// Prototypes and data structures for lazy GC to support pthread_detach().
int _thread_can_join(uint32_t id);
int _thread_can_destroy(uint32_t id);

typedef struct pthread_detached
{
    uint32_t tid;
    struct pthread_detached *next;
} pthread_detached_t;

pthread_detached_t *detach_head = NULL;

void _pthread_gc()
{
    /* Grab the detach head. Since we only ever place items onto the
     * beginning of it, we can safely lock here. We only need to relock
     * if we need to delete an entry. */
    uint32_t old_irq = irq_disable();
    pthread_detached_t *cur = detach_head;
    irq_restore(old_irq);

    pthread_detached_t *last = 0;
    while (cur)
    {
        // See if we can join this thread.
        if (_thread_can_join(cur->tid))
        {
            thread_join(cur->tid);
        }

        // See if we can nuke this thread entirely.
        if (_thread_can_destroy(cur->tid))
        {
            thread_destroy(cur->tid);

            // Remove the structure from our linked list.
            pthread_detached_t *next = cur->next;
            if (last)
            {
                last->next = next;
            }
            else
            {
                // Since we only ever add by changing the head, if something
                // changed the head that means we need to re-iterate.
                old_irq = irq_disable();
                if (detach_head == cur)
                {
                    detach_head = next;
                }
                else
                {
                    pthread_detached_t *todelete = detach_head;
                    while (todelete)
                    {
                        if (todelete->next == cur)
                        {
                            todelete->next = next;
                            break;
                        }

                        todelete = todelete->next;
                    }
                }
                irq_restore(old_irq);
            }

            // Kill the structure that tracks it.
            free(cur);
            cur = next;
        }
        else
        {
            // Just go to the next spot.
            last = cur;
            cur = cur->next;
        }
    }
}

int pthread_create(
    pthread_t *pthread,
    const pthread_attr_t *attr,
    void *(*start_routine)(void *),
    void *arg
) {
    int create_detached = 0;

    if (attr)
    {
        if (attr->is_initialized == 0)
        {
            return EINVAL;
        }
        if (attr->stackaddr != 0)
        {
            return EINVAL;
        }
        if (attr->stacksize != THREAD_STACK_SIZE)
        {
            return EINVAL;
        }
        if (attr->contentionscope != PTHREAD_SCOPE_SYSTEM)
        {
            return EINVAL;
        }

        // Only attribute we really support right now.
        create_detached = attr->detachstate == PTHREAD_CREATE_DETACHED;
    }

    uint32_t new_thread = thread_create("pthread", start_routine, arg);
    if (new_thread)
    {
        int retval = 0;

        if (create_detached)
        {
            // Create a new entry for this thread to be auto-cleaned.
            pthread_detached_t *new = malloc(sizeof(pthread_detached_t));
            if (new)
            {
                uint32_t old_irq = irq_disable();
                new->tid = new_thread;
                new->next = detach_head;

                // Slot it into the structure.
                detach_head = new;
                irq_restore(old_irq);
            }
            else
            {
                retval = ENOMEM;
            }
        }

        *pthread = (pthread_t)new_thread;
        thread_start(new_thread);
        _pthread_gc();
        return retval;
    }
    else
    {
        _pthread_gc();
        return EAGAIN;
    }
}

int pthread_join(pthread_t pthread, void **value_ptr)
{
    uint32_t old_irq = irq_disable();

    // We don't want to map substitutions back because we don't want to accidentally
    // join on the destructor thread. So just do a simple cast.
    uint32_t actual_tid = (uint32_t)pthread;

    if (thread_info(actual_tid, NULL))
    {
        // Verify that it is joinable.
        int retval = 0;
        pthread_detached_t *cur = detach_head;
        while (cur)
        {
            if (cur->tid == actual_tid)
            {
                // Thread is not joinable, it has been detached!
                retval = EINVAL;
                break;
            }

            cur = cur->next;
        }

        // Join and destroy the thread if it is joinable. We need to do this
        // with threads enabled as join is a syscall.
        irq_restore(old_irq);
        if (retval == 0)
        {
            void *ret = thread_join(actual_tid);
            thread_destroy(actual_tid);
            if (value_ptr)
            {
                *value_ptr = (ret == THREAD_CANCELLED ? PTHREAD_CANCELED : ret);
            }
        }

        _pthread_gc();
        return retval;
    }
    else
    {
        irq_restore(old_irq);
        _pthread_gc();
        return ESRCH;
    }
}

int pthread_detach(pthread_t pthread)
{
    uint32_t old_irq = irq_disable();

    // We don't want to map substitutions back because we don't want to accidentally
    // join on the destructor thread. So just do a simple cast.
    uint32_t actual_tid = (uint32_t)pthread;

    if (thread_info(actual_tid, NULL))
    {
        int retval = 0;
        pthread_detached_t *cur = detach_head;
        while (cur)
        {
            if (cur->tid == actual_tid)
            {
                // Thread has already been detached!
                retval = EINVAL;
                break;
            }

            cur = cur->next;
        }

        if (retval == 0)
        {
            // Create a new entry for this thread to be auto-cleaned.
            pthread_detached_t *new = malloc(sizeof(pthread_detached_t));
            if (new)
            {
                new->tid = actual_tid;
                new->next = detach_head;

                // Slot it into the structure.
                detach_head = new;
            }
            else
            {
                retval = ENOMEM;
            }
        }

        irq_restore(old_irq);
        _pthread_gc();
        return retval;
    }
    else
    {
        irq_restore(old_irq);
        _pthread_gc();
        return ESRCH;
    }
}

void pthread_yield(void)
{
    _pthread_gc();
    thread_yield();
}

void pthread_testcancel(void)
{
    _pthread_gc();
    thread_yield();
}

int pthread_setcancelstate(int state, int *oldstate)
{
    int old = thread_set_cancellable(pthread_self(), state == PTHREAD_CANCEL_ENABLE ? 1 : 0);
    if (oldstate)
    {
        *oldstate = (old != 0 ? PTHREAD_CANCEL_ENABLE : PTHREAD_CANCEL_DISABLE);
    }
    return 0;
}

int pthread_setcanceltype(int type, int *oldtype)
{
    int old = thread_set_cancelasync(pthread_self(), type == PTHREAD_CANCEL_ASYNCHRONOUS ? 1 : 0);
    if (oldtype)
    {
        *oldtype = (old != 0 ? PTHREAD_CANCEL_ASYNCHRONOUS : PTHREAD_CANCEL_DEFERRED);
    }
    return 0;
}

int pthread_cancel(pthread_t pthread)
{
    uint32_t old_irq = irq_disable();

    // We don't want to map substitutions back because we don't want to accidentally
    // join on the destructor thread. So just do a simple cast.
    uint32_t actual_tid = (uint32_t)pthread;

    int retval = 0;
    if (thread_info(actual_tid, NULL))
    {
        irq_restore(old_irq);
        thread_cancel(actual_tid);
        _pthread_gc();
    }
    else
    {
        irq_restore(old_irq);
        retval = ESRCH;
    }
    return retval;
}

void pthread_exit(void *value_ptr)
{
    thread_exit(value_ptr);
    __builtin_unreachable();
}

int pthread_getcpuclockid(pthread_t thread, clockid_t *clock_id)
{
    // We don't support per-CPU pthread clocks.
    return ENOENT;
}

int pthread_setconcurrency(int new_level)
{
    // We don't support concurrency levels, since we are single-cored.
    return EINVAL;
}

int pthread_getconcurrency(void)
{
    // We don't support concurrency levels, since we are single-cored.
    return EINVAL;
}

int pthread_atfork(void (*prepare)(void), void (*parent)(void), void (*child)(void))
{
    // We don't suppor fork(), so don't bother supporting this.
    return EINVAL;
}

int pthread_getattr_np(pthread_t pthread, pthread_attr_t *attr)
{
    uint32_t old_irq = irq_disable();

    // We don't want to map substitutions back because we don't want to accidentally
    // join on the destructor thread. So just do a simple cast.
    uint32_t actual_tid = (uint32_t)pthread;

    int retval = 0;

    if (thread_info(actual_tid, NULL))
    {
        memset(attr, 0, sizeof(pthread_attr_t));
        attr->is_initialized = 1;
        attr->stacksize = THREAD_STACK_SIZE;
        attr->contentionscope = PTHREAD_SCOPE_SYSTEM;
        attr->detachstate = PTHREAD_CREATE_JOINABLE;

        // Figure out if it is joinable or detached.
        pthread_detached_t *cur = detach_head;
        while (cur)
        {
            if (cur->tid == actual_tid)
            {
                // Thread is not joinable, it is detachable instead.
                attr->detachstate = PTHREAD_CREATE_DETACHED;
                break;
            }

            cur = cur->next;
        }
    }
    else
    {
        // Couldn't find thread.
        retval = ESRCH;
    }

    irq_restore(old_irq);
    return retval;
}

int pthread_spin_init (pthread_spinlock_t *spinlock, int pshared)
{
    if (spinlock == NULL)
    {
        return EINVAL;
    }

    // Use mutexes for spinlocks, but only ever use trylock in a spinloop.
    mutex_t *spinmutex = malloc(sizeof(mutex_t));
    if (spinmutex == 0)
    {
        return ENOMEM;
    }

    // Initialize the mutex.
    mutex_init(spinmutex);

    // Return a pointer aliased to the spinlock type.
    *spinlock = (pthread_spinlock_t)spinmutex;
    return 0;
}

int pthread_spin_destroy (pthread_spinlock_t *spinlock)
{
    if (spinlock == NULL)
    {
        return EINVAL;
    }

    // Alias back to a pointer.
    mutex_t *spinmutex = (mutex_t *)(*spinlock);

    // Free the underlying mutex.
    mutex_free(spinmutex);

    // Free the memory for the mutex.
    free(spinmutex);
    return 0;
}

int pthread_spin_lock (pthread_spinlock_t *spinlock)
{
    if (spinlock == NULL)
    {
        return EINVAL;
    }

    // Alias back to a pointer.
    mutex_t *spinmutex = (mutex_t *)(*spinlock);

    // Wait until we get the mutex.
    while(mutex_try_lock(spinmutex) == 0) { ; }

    // We got it!
    return 0;
}

int pthread_spin_trylock (pthread_spinlock_t *spinlock)
{
    if (spinlock == NULL)
    {
        return EINVAL;
    }

    // Alias back to a pointer.
    mutex_t *spinmutex = (mutex_t *)(*spinlock);

    if (mutex_try_lock(spinmutex))
    {
        // Got the mutex!
        return 0;
    }
    else
    {
        // Did not get the mutex!
        return EBUSY;
    }

}

int pthread_spin_unlock (pthread_spinlock_t *spinlock)
{
    if (spinlock == NULL)
    {
        return EINVAL;
    }

    // Alias back to a pointer.
    mutex_t *spinmutex = (mutex_t *)(*spinlock);

    // Unlock the mutex.
    mutex_unlock(spinmutex);
    return 0;
}

int pthread_mutexattr_init (pthread_mutexattr_t *attr)
{
    memset(attr, 0, sizeof(pthread_mutexattr_t));
    attr->is_initialized = 1;
    return 0;
}

int pthread_mutexattr_destroy (pthread_mutexattr_t *attr)
{
    attr->is_initialized = 0;
    return 0;
}

int pthread_mutexattr_getpshared (const pthread_mutexattr_t *attr, int *pshared)
{
    // We don't support pshared attributes, since we don't have processes.
    return EINVAL;
}

int pthread_mutexattr_setpshared (pthread_mutexattr_t *attr, int pshared)
{
    // We don't support pshared attributes, since we don't have processes.
    return EINVAL;
}

int pthread_mutexattr_gettype (const pthread_mutexattr_t *attr, int *kind)
{
    // All mutexes in libnaomi are recursive and checked, so return that.
    if (kind)
    {
        *kind = PTHREAD_MUTEX_RECURSIVE;
    }
    return 0;
}

int pthread_mutexattr_settype (pthread_mutexattr_t *attr, int kind)
{
    // Ignore this, since all mutexes are recursive and checked.
    return 0;
}

int pthread_mutex_init (pthread_mutex_t *mutex, const pthread_mutexattr_t *attr)
{
    if (mutex == NULL)
    {
        return EINVAL;
    }

    if (attr)
    {
        if (attr->is_initialized == 0)
        {
            return EINVAL;
        }
    }

    mutex_t *lnmutex = malloc(sizeof(mutex_t));
    if (lnmutex == 0)
    {
        return ENOMEM;
    }

    // Initialize the mutex.
    mutex_init(lnmutex);

    // Return a pointer aliased to the mutex type.
    *mutex = (pthread_mutex_t)lnmutex;
    return 0;
}

int pthread_mutex_destroy (pthread_mutex_t *mutex)
{
    if (mutex == NULL)
    {
        return EINVAL;
    }
    if (*mutex == PTHREAD_MUTEX_INITIALIZER)
    {
        return 0;
    }

    // Alias back to a pointer.
    mutex_t *lnmutex = (mutex_t *)(*mutex);

    // Free the underlying mutex.
    mutex_free(lnmutex);

    // Free the memory for the mutex.
    free(lnmutex);
    return 0;
}

int pthread_mutex_lock (pthread_mutex_t *mutex)
{
    if (mutex == NULL)
    {
        return EINVAL;
    }

    // First, figure out if we need to initialize this mutex.
    mutex_lock(&static_mutex);
    if (*mutex == PTHREAD_MUTEX_INITIALIZER)
    {
        pthread_mutex_init(mutex, NULL);
    }
    mutex_unlock(&static_mutex);

    if (*mutex == PTHREAD_MUTEX_INITIALIZER)
    {
        return EINVAL;
    }

    // Alias back to a pointer.
    mutex_t *lnmutex = (mutex_t *)(*mutex);

    // Lock the mutex.
    mutex_lock(lnmutex);
    return 0;
}

int pthread_mutex_trylock (pthread_mutex_t *mutex)
{
    if (mutex == NULL)
    {
        return EINVAL;
    }

    // First, figure out if we need to initialize this mutex.
    mutex_lock(&static_mutex);
    if (*mutex == PTHREAD_MUTEX_INITIALIZER)
    {
        pthread_mutex_init(mutex, NULL);
    }
    mutex_unlock(&static_mutex);

    if (*mutex == PTHREAD_MUTEX_INITIALIZER)
    {
        return EINVAL;
    }

    // Alias back to a pointer.
    mutex_t *lnmutex = (mutex_t *)(*mutex);

    // Lock the mutex.
    if (mutex_try_lock(lnmutex))
    {
        // Got the mutex!
        return 0;
    }
    else
    {
        // Failed to get the mutex.
        return EBUSY;
    }
}

int pthread_mutex_unlock (pthread_mutex_t *mutex)
{
    if (mutex == NULL || *mutex == PTHREAD_MUTEX_INITIALIZER)
    {
        return EINVAL;
    }

    // Alias back to a pointer.
    mutex_t *lnmutex = (mutex_t *)(*mutex);

    // Lock the mutex.
    mutex_unlock(lnmutex);
    return 0;
}

int pthread_once (pthread_once_t *__once_control, void (*__init_routine)(void))
{
    // First, validate inputs.
    if (__once_control == NULL || __init_routine == NULL)
    {
        return EINVAL;
    }

    // Now, disable threads to atomically read the control.
    uint32_t old_irq = irq_disable();

    // First, check if the structure itself is initialized.
    if (!__once_control->is_initialized)
    {
        irq_restore(old_irq);
        return EINVAL;
    }

    // Now, check whether we've done this already, and mark that it is done.
    int executed = __once_control->init_executed;
    __once_control->init_executed = 1;
    irq_restore(old_irq);

    // Now, if we haven't run it yet, execute it now!
    if (!executed)
    {
        __init_routine();
    }

    // Succeeded, so return that we did so, regardless of running the init function.
    return 0;
}

typedef struct
{
    pthread_key_t key;
    void (*destructor)(void *);
} pthread_tls_destructor_t;

// Our list for registered destructors.
static int tls_destructor_count = 0;
static pthread_tls_destructor_t *tls_destructor = 0;

typedef struct
{
    pthread_key_t key;
    uint32_t tid;
    const void *data;
} pthread_tls_t;

// Our list for thread-local storage.
static int tls_count = 0;
static pthread_tls_t *tls = 0;

void *_pthread_destructor(void *_unused)
{
    while (1)
    {
        // Potential work we might need to do, if we find a dead thread with a destructor
        // registered and a non-null value for its thread-local storage.
        pthread_key_t foundkey;
        uint32_t foundtid = 0;
        void (*founddestructor)(void *) = NULL;

        // First, we need an exclusive lock for finding any threads that exist or don't.
        // That way the memory we are iterating over doesn't change from some other thread
        // calling TLS functions.
        mutex_lock(&tls_mutex);

        for (int i = 0; i < tls_count; i++)
        {
            // First, is this thread even alive?
            if (thread_info(tls[i].tid, NULL))
            {
                // This thread is still alive, ignore it.
                continue;
            }

            // Second, does this key have a destructor?
            for (int j = 0; j < tls_destructor_count; j++)
            {
                if (tls_destructor[j].key == tls[i].key && tls_destructor[j].destructor != NULL)
                {
                    // It does! Let's try running the destructor for this.
                    foundkey = tls[i].key;
                    foundtid = tls[i].tid;
                    founddestructor = tls_destructor[j].destructor;
                    break;
                }
            }

            if (foundtid != 0)
            {
                // Break out early if we found something to do.
                break;
            }
        }

        // Now, unlock since we either have some work to do (that we know won't be modified
        // because that thread is dead), or we have nothing to do.
        mutex_unlock(&tls_mutex);

        // If we have something to do, we need to be really careful to do this according to
        // spec. We should only be running the destructor if the current value is non-null,
        // and only after setting the value to null, but passing the old value to the destructor.
        // We also need to spoof the thread for the TLS functions to work at all (especially in
        // the destructor itself which is under the impression that it is in the original thread).
        if (foundtid != 0)
        {
            // Override our local thread ID from the perspective of pthreads.
            uint32_t old_irq = irq_disable();
            overridden_thread_id = thread_id();
            substituted_thread_id = foundtid;
            irq_restore(old_irq);

            // Now its safe to grab the old value. If it is null, just delete it and
            // move on with our lives.
            void *oldvalue = pthread_getspecific(foundkey);
            if (oldvalue != NULL)
            {
                // First, set the value to null so that we can detect if the thread
                // changed it in the destructor.
                pthread_setspecific(foundkey, NULL);

                // Now, call the destructor with the old value.
                founddestructor(oldvalue);

                // Now, see if it is still null. If it is not, we will do this again
                // on the next iteration. If it is, we can delete it and move on.
                // This can result in an infinite loop if a destructor sets a value
                // again and again, but the pthreads spec specifically allows for this.
                if (pthread_getspecific(foundkey) == NULL)
                {
                    pthread_key_delete(foundkey);
                }
            }
            else
            {
                // No need to run the destructor, just delete the key so we never see
                // it again.
                pthread_key_delete(foundkey);
            }

            // Restore our local thread ID from the perspective of pthreads.
            old_irq = irq_disable();
            overridden_thread_id = 0;
            substituted_thread_id = 0;
            irq_restore(old_irq);
        }
        else
        {
            // No work to be done, wait a second and try again.
            thread_sleep(1000000);
        }
    }
}

void _pthread_destructor_spawn()
{
    // Spawn a destructor thread which will look for data that needs deleting from
    // threads that have exited.
    uint32_t new_thread = thread_create("pthread destructor", _pthread_destructor, NULL);
    if (new_thread)
    {
        thread_start(new_thread);
    }
}

// The current global key for pthread keys.
static uint32_t tls_key = 1;

int pthread_key_create (pthread_key_t *__key, void (*__destructor)(void *))
{
    // First, do some quick invariant checks.
    if (__key == 0)
    {
        return EINVAL;
    }

    if (_irq_is_disabled(_irq_get_sr()))
    {
        _irq_display_invariant("pthread failure", "cannot create a tls key with threads disabled!");
    }

    // First, we need a mutex for our malloc setup.
    mutex_lock(&tls_mutex);

    // Now, grab a new global key for thread local storage.
    uint32_t new_key = tls_key++;

    if (__destructor != 0)
    {
        pthread_once(&destructor_once, _pthread_destructor_spawn);
        pthread_tls_destructor_t *new_tls_destructor = 0;

        if (tls_destructor_count == 0)
        {
            tls_destructor_count ++;
            new_tls_destructor = malloc(sizeof(pthread_tls_destructor_t) * tls_destructor_count);
        }
        else
        {
            tls_destructor_count ++;
            new_tls_destructor = realloc(tls_destructor, sizeof(pthread_tls_destructor_t) * tls_destructor_count);
        }

        if (new_tls_destructor == NULL)
        {
            return ENOMEM;
        }
        else
        {
            tls_destructor = new_tls_destructor;
        }

        // Copy the data in.
        tls_destructor[tls_destructor_count - 1].key = (pthread_key_t)new_key;
        tls_destructor[tls_destructor_count - 1].destructor = __destructor;
    }

    // Now, set up the key.
    *__key = (pthread_key_t)new_key;

    // We're done!
    mutex_unlock(&tls_mutex);
    return 0;
}

int pthread_setspecific (pthread_key_t __key, const void *__value)
{
    if (_irq_is_disabled(_irq_get_sr()))
    {
        _irq_display_invariant("pthread failure", "cannot set data to a tls key with threads disabled!");
    }

    // First, we need a mutex for our malloc setup.
    mutex_lock(&tls_mutex);

    // Now we need to find if there is any data for this key.
    uint32_t tid = pthread_self();
    for (int i = 0; i < tls_count; i++)
    {
        if (tls[i].key == __key && tls[i].tid == tid)
        {
            // Found it! Set the new value.
            tls[i].data = __value;
            mutex_unlock(&tls_mutex);
            return 0;
        }
    }

    // We didn't find it, so create a new entry and put the value in.
    pthread_tls_t *new_tls = 0;

    if (tls_count == 0)
    {
        tls_count ++;
        new_tls = malloc(sizeof(pthread_tls_t) * tls_count);
    }
    else
    {
        tls_count ++;
        new_tls = realloc(tls, sizeof(pthread_tls_t) * tls_count);
    }

    if (new_tls == NULL)
    {
        return ENOMEM;
    }
    else
    {
        tls = new_tls;
    }

    // Copy the data in.
    tls[tls_count - 1].key = __key;
    tls[tls_count - 1].tid = tid;
    tls[tls_count - 1].data = __value;

    // Success, set the data.
    mutex_unlock(&tls_mutex);
    return 0;
}

void *pthread_getspecific (pthread_key_t __key)
{
    if (_irq_is_disabled(_irq_get_sr()))
    {
        _irq_display_invariant("pthread failure", "cannot get data from a tls key with threads disabled!");
    }

    // First, we need a mutex for our malloc setup.
    mutex_lock(&tls_mutex);

    // Now we need to find if there is any data for this key.
    uint32_t tid = pthread_self();

    for (int i = 0; i < tls_count; i++)
    {
        if (tls[i].key == __key && tls[i].tid == tid)
        {
            // Found it! Set the new value.
            const void *value = tls[i].data;
            mutex_unlock(&tls_mutex);
            return (void *)value;
        }
    }

    // Couldn't find anything, so no data here.
    mutex_unlock(&tls_mutex);
    return 0;
}

int pthread_key_delete (pthread_key_t __key)
{
    if (_irq_is_disabled(_irq_get_sr()))
    {
        _irq_display_invariant("pthread failure", "cannot delete a tls key with threads disabled!");
    }

    // First, we need a mutex for our malloc setup.
    mutex_lock(&tls_mutex);

    // Now, we need to create a new structure containing everything that wasn't
    // in this thread's localstorage for this key.
    pthread_tls_t *new_tls = 0;
    int new_tls_count = 0;
    uint32_t tid = pthread_self();

    for (int i = 0; i < tls_count; i++)
    {
        if (tls[i].key == __key && tls[i].tid == tid)
        {
            // Don't keep this key around, continue so we don't copy it over.
            continue;
        }

        // Allocate space for this key to be kept around.
        if (new_tls_count == 0)
        {
            new_tls_count ++;
            new_tls = malloc(sizeof(pthread_tls_t) * new_tls_count);
        }
        else
        {
            new_tls_count ++;
            new_tls = realloc(new_tls, sizeof(pthread_tls_t) * new_tls_count);
            if (new_tls == NULL)
            {
                _irq_display_invariant("memory failure", "could not allocate memory when performing tls key garbage collection!");
            }
        }

        // Copy the data in.
        new_tls[new_tls_count - 1].key = tls[i].key;
        new_tls[new_tls_count - 1].tid = tls[i].tid;
        new_tls[new_tls_count - 1].data = tls[i].data;
    }

    // Free the old chunk of data, set the new one as boss.
    free(tls);
    tls = new_tls;
    tls_count = new_tls_count;

    // We finished, lets bail!
    mutex_unlock(&tls_mutex);
    return 0;
}
