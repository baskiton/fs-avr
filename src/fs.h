#ifndef FS_H
#define FS_H

#include <stdint.h>
#include <spi.h>

#include "dirent.h"
#include "block_dev.h"

struct vol_ops {
    void (*create)(void);
    void (*lookup)(void);
    void (*mkdir)(void);
    void (*rmdir)(void);
    void (*rename)(void);
    void (*setattr)(void);
    void (*getattr)(void);
    void (*update_time)(void);
};

typedef struct fs_volume_s fs_volume_t;

struct fs_volume_s {
    fs_volume_t *next;      // next volume in chain
    uint8_t v_num;          // volume number (0-127)
    uint32_t start_sector;  // First sector of the volume on the device.
    uint32_t tot_sectors;   // Total number of sectors
    uint8_t fs_type;        // File System type
    bdev_t *bdev;           // Block device
    void *fs_spec;          // FS specified data
    const struct vol_ops *v_ops;
    DIR root;               // root path
};

int8_t get_root(DIR *restrict dir, const fs_volume_t *restrict vol);
void set_pwd(const char *path);
void get_pwd(DIR *dir);

int8_t volumes_determine(spi_dev_t *dev);

#endif  /* !FS_H */
