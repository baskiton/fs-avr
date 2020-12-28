/*
    dirent.h - format of directory entries
*/

#ifndef SYS_DIRENT_H
#define SYS_DIRENT_H

#include <stdint.h>

typedef struct fs_volume_s fs_volume_t;

typedef struct __dirstream {
    fs_volume_t *vol;
    uint32_t clust;     // start cluster
    uint32_t sect;      // current sector
    uint8_t offset;     // current offset in entries
    void *entry;        // pointer to the directory entry
    uint8_t ent_size;   // entry size
    char *name;         // pointer to dir name
} DIR;

struct dirent {
    // ino_t d_ino;    // File serial number
    char *d_name;   // Filename string of entry
};

// int8_t alphasort(const struct dirent **d1, const struct dirent **d2);
int8_t closedir(DIR *dirp);
// int dirfd(DIR *dirp);
// DIR *fdopendir(int fd);
DIR *opendir(const char *dirname);
struct dirent *readdir(DIR *dirp);
// int8_t readdir_r(DIR *restrict dirp,
//                  struct dirent *restrict entry,
//                  struct dirent **restrict result);
void rewinddir(DIR *dirp);
// int8_t scandir(const char *dir,
//                struct dirent ***namelist,
//                int (*sel)(const struct dirent *),
//                int (*compar)(const struct dirent **,
//                const struct dirent **));
// void seekdir(DIR *dirp, long lock);
// long telldir(DIR *dirp);

#endif  /* !SYS_DIRENT_H */
