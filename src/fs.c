#include <avr/pgmspace.h>

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <ctype.h>

#include <spi.h>

#include "fs.h"
#include "block_dev.h"

//============== MBR ==================

/* MBR FSID */
#define EMPTY   0x00    // No partition
#define FAT12   0x01    // FAT12
#define FAT16   0x04    // FAT16 with less than 65536 sectors (< 32 MB)
#define FAT16B  0x06    // FAT16B with 65536 or more sectors (>= 32 MB)
#define NTOS    0x07    // IFS, HPFS, NTFS, exFAT, QNX
#define FAT32   0x0B    // FAT32 with CHS addressing
#define FAT32X  0x0C    // FAT32 with LBA addressing
#define FAT16X  0x0E    // FAT16B with LBA addressing, VFAT
#define EFI     0xEE    // EFI part (GPT)

/* MBR Partition Entry */
typedef struct __attribute__((packed)) mbr_partition_entry_s {
    uint8_t ap_flag;        // Active Partition Flag (0x80 is active, 0x00)
    uint8_t start_chs[3];   // CHS address of first absolute sector in partition
    uint8_t fs_id;          // File System ID (partition type)
    uint8_t end_chs[3];     // CHS address of last absolute sector in partition
    uint32_t start_lba;     // LBA of first absolute sector in the partition
    uint32_t total_sectors; // Number of sectors in partition
} mbr_part_t;

/* Master Boot Record (MBR) */
typedef struct __attribute__((packed)) mbr_s {
    union {
        struct __attribute__((packed)) {
            uint8_t code[440];  // Bootstrap Code area
            uint32_t sn;        // Disk Serial Number
            uint16_t reserved;
        } mbr_gen;
        struct __attribute__((packed)) {
            uint8_t code_1[218];    // Bootstrap code area (part 1)
            uint16_t zero;          // 0x0000
            uint8_t drive;          // 0x80 - 0xFF
            uint8_t seconds;        // 0-59
            uint8_t minutes;        // 0-59
            uint8_t hours;          // 0-23
            uint8_t code_2[216];    // Bootstrap code area (part 2)
            uint32_t sign;          // Disk Signature (Optional)
            uint16_t sign_protect;  // 0x0000 (0x5A5A if copy-protected)
        } mbr_mod;
    };
    mbr_part_t partition[4];
    uint16_t boot_signature;    // 0xAA55 Boot Signature
} mbr_t;

//============== !MBR =================

//============== GPT ==================

/* GPT Partition Entries */
typedef struct __attribute__((packed)) gpt_partition_entry_s {
    uint8_t part_type_guid[16];
    uint8_t uniq_part_guid[16];
    uint64_t first_lba;
    uint64_t last_lba;
    uint64_t attrib_flags;
    __CHAR16_TYPE__ partition_name[36]; // UTF-16LE code units
} gpt_part_t;

/* GUI Partition Table (GPT) */
typedef struct __attribute__((packed)) gpt_s {
    char signature[8];  // 'EFI PART'
    uint32_t revision;
    uint32_t hdr_size;              // header size (92 or 0x5C)
    uint32_t hdr_crc;               // header CRC32
    uint32_t reserved_0;            // must be 0
    uint64_t current_lba;           // LBA 1
    uint64_t backup_lba;            // header copy (LBA -1 - last sector)
    uint64_t first_usable_lba;
    uint64_t last_usable_lba;
    uint8_t disk_guid[16];
    uint64_t starting_lba_entries;  // LBA of partition table
    uint32_t entries_num;           // partitions number
    uint32_t entry_syze;
    uint32_t part_entry_crc;        // CRC32 of part table
    uint8_t reserved_1[420];        // must be 0
} gpt_t;

//============== !GPT =================

typedef union fs_cache {
    mbr_t mbr;
    gpt_t gpt;
    uint8_t data[512];
} fs_cache_t;

extern int8_t fat_init(fs_volume_t *vol, req_t *req);

int8_t fs_follow_path(DIR *restrict dir,
                      const char *restrict path,
                      uint8_t flags);

static struct {
    fs_volume_t *next;
} vol_tbl = {0};    // volumes table

static DIR pwd = {0};

/*!
 * @return volume number; -1 on error (if vol_num > 127)
 */
static int8_t vtable_append(fs_volume_t *vol) {
    fs_volume_t *node = (void *)&vol_tbl;
    int8_t num = 0;

    while (node->next != NULL) {
        node = node->next;
        if (node->v_num == 127)
            return -1;
        num = node->v_num + 1;
    }
    node->next = vol;
    vol->next = NULL;
    vol->v_num = num;

    return num;
}

/*!
 * @brief Insert volume to volume table by his number
 * @param vol Volume with number
 * @return v_num on success; -1 on fail
 */
static int8_t vtable_insert_vol(fs_volume_t *vol) {
    fs_volume_t *node = (void *)&vol_tbl;
    fs_volume_t *prev = node;

    if (vol->v_num > 127 || vol->v_num < 0)
        return -1;

    while (node->next != NULL) {
        prev = node;
        node = node->next;
        if (node->v_num == vol->v_num)
            // this volume number is already exist
            return -1;
        if (node->v_num > vol->v_num) {
            node = prev;
            break;
        }
    }

    vol->next = node->next;
    node->next = vol;

    return vol->v_num;
}

static void vtable_del_vol(uint8_t num) {
    fs_volume_t *node = (void *)&vol_tbl;
    fs_volume_t *to_del;

    if (num > 127 || num < 0)
        return;

    while (node->next != NULL) {
        if (node->next->v_num != num) {
            node = node->next;
            continue;
        }
        to_del = node->next;
        node->next = to_del->next;
        free(to_del);
        break;
    }
}

/*!
 * @brief Get volume by number
 * @param num Vol number. max=127
 */
static fs_volume_t *vtable_get_vol(uint8_t num) {
    fs_volume_t *node = (void *)&vol_tbl;

    if (num > 127)
        return NULL;

    while (node->next != NULL) {
        if (node->next->v_num != num) {
            node = node->next;
            continue;
        }
        return node->next;
    }

    return NULL;
}

/*!
 * @brief Get volume number by string
 * @param str String (e.g. "12:/my/path/to/file")
 * @return 0-127 - volume number; -1 on error
 */
static int8_t get_vol_num_by_str(const char *str) {
    long num;
    char *endptr;

    num = strtol(str, &endptr, 10);
    if (((num == 0) && (endptr == str)) ||
        (num > 127) || (num < 0) || (*endptr++ != ':') ||
        ((*endptr != '/') && (*endptr != '\\')))
        // is not volume number
        return -1;

    return (int8_t)(num & 0xFF);
}

/*!
 * @brief Get root directory of specified volume
 * @param dir Directory entry to store root
 * @param vol Specified volume
 * @return 0 on success
 */
int8_t get_root(DIR *restrict dir, const fs_volume_t *restrict vol) {
    if (!vol)
        return -1;

    memcpy(dir, &vol->root, sizeof(DIR));

    return 0;
}

/*!
 * @brief Set Present Work Directory (PWD) by path
 * @param vol Volume to set PWD
 * @param path Full path
 */
void set_pwd(const char *path) {
    int8_t err;
    DIR new_pwd = {0};

    if (!path)  // root of "0:/>"
        err = get_root(&new_pwd, vtable_get_vol(0));
    else
        err = fs_follow_path(&new_pwd, path, 0);

    if (!err)
        return;

    if (pwd.vol && pwd.name)
        free(pwd.name);

    memcpy(&pwd, &new_pwd, sizeof(DIR));

    return;
}

void get_pwd(DIR *dir) {
    if (pwd.vol)
        memcpy(dir, &pwd, sizeof(DIR));
}

static int8_t get_start_entry(const char *restrict *path,
                              DIR *restrict dir) {
    const char *cp = *path;

    if (dir->vol)   // start entry already present
        return 0;

    get_pwd(dir);
    if (!dir->vol)
        // PWD is not set
        return -1;

    if ((cp[0] == '/') || (cp[0] == '\\') ) {
        /** TODO: */
        return get_root(dir, dir->vol);
    } else if (isdigit(cp[0])) {
        /** TODO: */
        int8_t vol_num = get_vol_num_by_str(cp);

        if (vol_num >= 0)
            /** TODO: */
            return get_root(dir, vtable_get_vol(vol_num));

        // if vol_num < 0, path is pwd
    }

    return 0;
}

int8_t fs_follow_path(DIR *restrict dir,
                      const char *restrict path,
                      uint8_t flags) {
    int8_t ret = -1;
    char *node;

    // check if path == NULL or path[0] == '\0' or dir == NULL
    if (!path || !*path || !dir)
        return ret;

    ret = get_start_entry(&path, dir);

    // node = strsep_P(&path, PSTR("\\/"));

    return -1;
}

static int8_t v_det(bdev_t *bdev) {
    int8_t ret;
    fs_cache_t cache;
    req_f req_func;
    req_t req;
    mbr_part_t part_list[4];
    mbr_part_t *part = part_list;

    req_func = pgm_read_ptr(&bdev->blk_ops->request);
    if (!req_func)
        // no request function
        return -1;

    // fill request
    req.bdev = bdev;
    req.cmd_flags = REQ_READ;
    req.block = 0;
    req.buf = &cache;

    ret = req_func(&req);
    if (ret)
        // some err
        return ret;

    if (cache.mbr.boot_signature != 0xAA55) {
        // not mapped
        printf_P(PSTR("storage is not mapped\n"));
        return 0;
    }

    // copying the list of partitions to free the cache
    memcpy(part_list, cache.mbr.partition, sizeof(part_list));

    for (uint8_t i = 0; i < 4; i++, part++) {
        fs_volume_t *vol;

        if (((part->ap_flag != 0x80) && (part->ap_flag != 0x00)) ||
            (part->total_sectors == 0) || (part->start_lba == 0) ||
            (part->fs_id == EMPTY)) {
            // empty part. skip
            continue;
        }

        vol = malloc(sizeof(fs_volume_t));
        if (!vol)
            // ENOMEM
            return -1;

        vol->start_sector = part->start_lba;
        vol->tot_sectors = part->total_sectors;
        vol->fs_type = part->fs_id;
        vol->bdev = bdev;

        switch (part->fs_id) {
            case FAT12:
            case FAT16:
            case FAT16B:
            case FAT32:
            case FAT32X:
            case FAT16X:
                ret = fat_init(vol, &req);
                break;

            case NTOS:
                // not supported yet. fall through

            case EFI:
                /** TODO: GPT
                req.block = 1;
                err = req_func(&req);
                if (!memcmp_P(cache.gpt.signature, PSTR("EFI PART"), 8)) {
                    // check all CRC
                    // ...
                    // is GPT
                    printf_P(PSTR("is GPT. Not supported yet\n"));
                    return -1;
                }
                // */
                // not supported yet. fall through

            default:    // unknown/unsupported fs
                free(vol);
                continue;
        }

        if (ret) {
            // some error
            free(vol);
            continue;
        }

        vtable_append(vol);

        {   // setting root name
            vol->root.name = malloc(6);
            if (!vol->root.name) {
                // ENOMEM
                ret = -1;
                vtable_del_vol(vol->v_num);
                free(vol);
                continue;
            }
            sprintf_P(vol->root.name, PSTR("%u:/"), vol->v_num);
        }

        if (vol->v_num == 0)
            set_pwd(NULL);  // set PWD as root of "0:/>"

        printf_P(PSTR("Vol %u; fs 0x%02X\n"), vol->v_num, vol->fs_type);
    }

    return ret;
}

/*!
 * @brief Search, determine and mount volumes on the drive
 * @param dev SPI device (storage class)
 * @return Number of volumes found and mounted; -1 on error
 */
int8_t volumes_determine(spi_dev_t *dev) {
    return v_det(spi_get_priv(dev));
}
