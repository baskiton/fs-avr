#ifndef SYS_STAT_H
#define SYS_STAT_H

#include <stdint.h>
#include <sys/types.h>
#include <time.h>

#ifndef mod_t
typedef uint32_t mode_t;
#endif  /* mod_t */

#ifndef nlink_t
typedef uint16_t nlink_t;
#endif  /* nlink_t */

/* Type of file */
#define S_IFMT   0170000    // Mask
#define S_IFBLK  0060000    // Block device
#define S_IFCHR  0020000    // Character device
#define S_IFIFO  0010000    // FIFO special
#define S_IFREG  0100000    // Regular file
#define S_IFDIR  0040000    // Directory
#define S_IFLNK  0120000    // Symbolic link
#define S_IFSOCK 0140000    // Socket

/* Test macros for file types */
#define __S_IS_TYPE(mode, mask) (((mode) & S_IFMT) == (mask))
#define S_ISBLK(m) __S_IS_TYPE((m), S_IFBLK)    // Test for a block special file
#define S_ISCHR(m) __S_IS_TYPE((m), S_IFCHR)    // Test for a character special file
#define S_ISFIFO(m) __S_IS_TYPE((m), S_IFIFO)   // Test for a pipe or FIFO special file
#define S_ISREG(m) __S_IS_TYPE((m), S_IFREG)    // Test for a regular file
#define S_ISDIR(m) __S_IS_TYPE((m), S_IFDIR)    // Test for a directory
#define S_ISLNK(m) __S_IS_TYPE((m), S_IFLNK)    // Test for a symbolic link
#define S_ISSOCK(m) __S_IS_TYPE((m), S_IFSOCK)  // Test for a socket

/* mode_t */
#define S_ISUID 04000   // Set-user-ID on execution
#define S_ISGID 02000   // Set-group-ID on execution
#define S_ISVTX 01000   // On directories, restricted deletion flag
#define S_IRUSR 0400    // Read permission, owner
#define S_IWUSR 0200    // Write permission, owner
#define S_IXUSR 0100    // Execute/search permission, owner
#define S_IRWXU (S_IRUSR | S_IWUSR | S_IXUSR)   // Read, write, execute/search by owner
#define S_IRGRP 040     // Read permission, group
#define S_IWGRP 020     // Write permission, group
#define S_IXGRP 010     // Execute/search permission, group
#define S_IRWXG (S_IRGRP | S_IWGRP | S_IXGRP)   // Read, write, execute/search by group
#define S_IROTH 04      // Read permission, others
#define S_IWOTH 02      // Write permission, others
#define S_IXOTH 01      // Execute/search permission, others
#define S_IRWXO (S_IROTH | S_IWOTH | S_IXOTH)   // Read, write, execute/search by others

/* Macros for common mode bit masks */
#define ACCESSPERMS (S_IRWXU | S_IRWXG | S_IRWXO)   // 0777
#define ALLPERMS (S_ISUID | S_ISGID | S_ISVTX | ACCESSPERMS)    // 07777
#define DEFFILEMODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH) // 0666

/* Always evaluate to 0 */
#define S_TYPEISMQ(buf) ((buf)->st_mode - (buf)->st_mode)   // Test for a message queue
#define S_TYPEISSEM(buf) ((buf)->st_mode - (buf)->st_mode)  // Test for a semaphore
#define S_TYPEISSHM(buf) ((buf)->st_mode - (buf)->st_mode)  // Test for a shared memory object

/* for use with the futimens() and utimensat() */
#define UTIME_NOW ((1l << 30) - 1l)
#define UTIME_OMIT ((1l << 30) - 2l)

struct stat {
    // dev_t st_dev;       // Device ID of device containing file
    // ino_t st_ino;       // File serial number
    mode_t st_mode;     // Mode of file
    nlink_t st_nlink;   // Number of hard links to the file
    // uid_t st_uid;       // User ID of file
    // gid_t st_gid;       // Group ID of file

    /* [XSI]
    dev_t st_rdev;      // Device ID (if file is character or block special)
    //  */

    off_t st_size;      // size of file, in bytes

    time_t st_atime;    // Last data access timestamp (seconds)
    time_t st_mtime;    // Last data modification timestamp (seconds)
    time_t st_ctime;    // Last file status change timestamp (seconds)

    /* [XSI]
    blksize_t st_blksize;   // A file system-specific preferred I/O block size
                            // for this object. In some file system types, this
                            // may vary from file to file.
    blkcnt_t st_blocks;     // Number of blocks allocated for this object
    //  */
};

// int8_t chmod(const char *path, mode_t mode);
// int8_t fchmod(int fildes, mode_t mode);
// int8_t fchmodat(int fd, const char *path, mode_t mode, int flag);
// int8_t fstat(int fildes, struct stat *buf);
// int8_t fstatat(int fd, const char *restrict path, struct stat *restrict buf, int flag);
// int8_t futimens(int fd, const struct timespec times[2]);
// int8_t lstat(const char *restrict path, struct stat *restrict buf);
// int8_t mkdir(const char *path, mode_t mode);
// int8_t mkdirat(int fd, const char *path, mode_t mode);
// int8_t mkfifo(const char *path, mode_t mode);
// int8_t mkfifoat(int fd, const char *path, mode_t mode);
/* [XSI]
int8_t mknod(const char *path, mode_t mode, dev_t dev);
int8_t mknodat(int fd, const char *path, mode_t mode, dev_t dev);
//  */

int8_t stat(const char *restrict path, struct stat *restrict buf);
// mode_t umask(mode_t cmask);
// int8_t utimensat(int fd, const char *path, const struct timespec times[2], int flag);

#endif  /* !SYS_STAT_H */
