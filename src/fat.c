#include <avr/pgmspace.h>

#include <stdint.h>
#include <stdlib.h>

#include "fs.h"

/* FAT types */
#define FAT12 0 // FAT12
#define FAT16 1 // FAT16
#define FAT32 2 // FAT32
#define FAT64 3 // exFAT

/* EOC */
#define FAT12_EOC 0x0FF8
#define FAT16_EOC 0xFFF8
#define FAT32_EOC 0x0FFFFFF8

/* Bad cluster value */
#define FAT12_BAD 0x0FF7
#define FAT16_BAD 0xFFF7
#define FAT32_BAD 0x0FFFFFF7

struct __attribute__((packed)) BPB_s {
    uint8_t BS_jmpBoot[3];      // Jump instruction to boot code
    uint8_t BS_OEMName[8];      // Name string (e.g. "MSWIN4.1")
    uint16_t BPB_BytsPerSec;    // Count of bytes per sector
    uint8_t BPB_SecPerClus;     // Number of sectors per allocation unit
    uint16_t BPB_RsvdSecCnt;    // Number of reserved sectors in the Reserved region of the volume starting at the first sector of the volume. This field must not be 0. 
    uint8_t BPB_NumFATs;        // The count of FAT data structures on the volume
    uint16_t BPB_RootEntCnt;    // For FAT12 and FAT16, this field contains the count of 32-byte directory entries in the root directory. For FAT32, this field must be set to 0
    uint16_t BPB_TotSec16;      // This field is the old 16-bit total count of sectors on the volume
    uint8_t BPB_Media;          // 0xF8 is the standard value for “fixed” (non-removable) media
    uint16_t BPB_FATSz16;       // This field is the FAT12/FAT16 16-bit count of sectors occupied by ONE FAT. On FAT32 this field must be 0, and BPB_FATSz32 contains the FAT size count
    uint16_t BPB_SecPerTrk;     // Sectors per track for interrupt 0x13
    uint16_t BPB_NumHeads;      // Number of heads for interrupt 0x13
    uint32_t BPB_HiddSec;       // Count of hidden sectors preceding the partition that contains this FAT volume
    uint32_t BPB_TotSec32;      // This field is the new 32-bit total count of sectors on the volume

    union {
        struct __attribute__((packed)) {
            uint8_t BS_DrvNum;          // Int 0x13 drive number (e.g. 0x80)
            uint8_t BS_Reserved1;       // Reserved (used by Windows NT)
            uint8_t BS_BootSig;         // Extended boot signature (0x29)
            uint32_t BS_VolID;          // Volume serial number
            uint8_t BS_VolLab[11];      // Volume label
            uint8_t BS_FilSysType[8];   // One of the strings "FAT12   ", "FAT16   ", or "FAT     "
            uint8_t BS_bootCode[448];   // Bootstrap code
        } fat12_16_ext;
        struct __attribute__((packed)) {
            uint32_t BPB_FATSz32;       // Count of sectors occupied by ONE FAT. BPB_FATSz16 must be 0
            uint16_t BPB_ExtFlags;      // Flags
            uint16_t BPB_FSVer;         // Version number of the FAT32 volume (maj.min)
            uint32_t BPB_RootClus;      // This is set to the cluster number of the first cluster of the root directory, usually 2 but not required to be 2
            uint16_t BPB_FSInfo;        // Sector number of FSINFO structure in the reserved area. Usually 1
            uint16_t BPB_BkBootSec;     // If non-zero, indicates the sector number in the reserved area of the volume of a copy of the boot record. Usually 6. No value other than 6 is recommended.
            uint8_t BPB_Reserved[12];   // Reserved for future expansion
            uint8_t BS_DrvNum;          // Int 0x13 drive number (e.g. 0x80)
            uint8_t BS_Reserved1;       // Reserved (used by Windows NT)
            uint8_t BS_BootSig;         // Extended boot signature (0x29)
            uint32_t BS_VolID;          // Volume serial number
            uint8_t BS_VolLab[11];      // Volume label
            uint8_t BS_FilSysType[8];   // Always set to the string "FAT32   "
            uint8_t BS_bootCode[420];   // Bootstrap code
        } fat32_ext;
    };
    uint16_t BS_signature;  // must be 0xAA55
};

/* FS Info - Only for FAT32 */
struct __attribute__((packed)) FSInfo_s {
    uint32_t FSI_LeadSig;       // Lead signature (must be 0x41615252 to indicate a valid FSInfo structure)
    uint8_t FSI_Reserved1[480];
    uint32_t FSI_StrucSig;      // Another signature (must be 0x61417272)
    uint32_t FSI_Free_Count;    // Contains the last known free cluster count on the volume
    uint32_t FSI_Nxt_Free;      // Indicates the cluster number at which the filesystem driver should start looking for available clusters
    uint8_t FSI_Reserved2[12];
    uint32_t FSI_TrailSig;      // Trail signature (0xAA550000)
};

struct __attribute__((packed)) FATDir_s {
    uint8_t DIR_Name[11];       // Short name (8.3 format)
    uint8_t DIR_Attr;           // File attributes:
#define ATTR_READ_ONLY  0x01
#define ATTR_HIDDEN     0x02
#define ATTR_SYSTEM     0x04
#define ATTR_VOLUME_ID  0x08
#define ATTR_DIRECTORY  0x10
#define ATTR_ARCHIVE    0x20
#define ATTR_LONG_NAME (ATTR_READ_ONLY | ATTR_HIDDEN |  \
                        ATTR_SYSTEM | ATTR_VOLUME_ID)
    uint8_t DIR_NTRes;          // Reserved for use by Windows NT
    uint8_t DIR_CrtTimeTenth;   // Millisecond stamp at file creation time
    uint16_t DIR_CrtTime;       // Time file was created
    uint16_t DIR_CrtDate;       // Date file was created
    uint16_t DIR_LstAccDate;    // Last access date
    uint16_t DIR_FstClusHI;     // High word of this entry’s first cluster number (always 0 for a FAT12 or FAT16 volume)
    uint16_t DIR_WrtTime;       // Time of last write
    uint16_t DIR_WrtDate;       // Date of last write
    uint16_t DIR_FstClusLO;     // Low word of this entry’s first cluster number
    uint32_t DIR_FileSize;      // 32-bit DWORD holding this file’s size in bytes
};

struct __attribute__((packed)) FATLDir_s {
    uint8_t LDIR_Ord;           // The order of this entry in the sequence of long dir entries
    uint16_t LDIR_Name1[5];     // Characters 1-5
    uint8_t LDIR_Attr;          // Must be ATTR_LONG_NAME
    uint8_t LDIR_Type;          // If zero, indicates a directory entry that is a sub-component of a long name
    uint8_t LDIR_Chksum;        // Checksum of name in the short dir entry at the end of the long dir set
    uint16_t LDIR_Name2[6];     // Characters 6-11
    uint16_t LDIR_FstClusLO;    // Must be ZERO
    uint16_t LDIR_Name3[2];     // Characters 12-13
};

typedef struct BPB_s bpb_t;
typedef struct FSInfo_s fs_info_t;
typedef struct FATDir_s dir_t;
typedef struct FATLDir_s ldir_t;

typedef union fat_cache {
    bpb_t bpb;
    fs_info_t fs_info;
    dir_t dir[16];
    uint32_t fat32[128];
    uint16_t fat16[256];
    uint8_t data[512];
} fat_cache_t;

struct fat_entry_ops {

};

typedef struct fat_spec_data_s {
    uint8_t bytes_per_sec_log;  // bytes per sector (log_2)
    uint8_t sec_per_clst_log;   // sectros per cluster (log_2)
    uint32_t tot_clusters;  // total clusters number
    uint32_t sec_per_fat;   // sectors per FAT
    uint8_t fat_number;     // number of FAT
    uint32_t fat_sector;    // FAT sector
    uint32_t root_sector;   // root sector
    uint32_t data_sector;   // sector for cluster #2
    uint8_t fat_type;       // FAT12, FAT16 or FAT32
    const struct fat_entry_ops *ent_ops;
} fat_spec_t;

static uint8_t log_2(uint32_t num) {
    uint8_t ret = 0;

    if (num) {
        while (num != 1) {
            num >>= 1;
            ret++;
        }
    }

    return ret;
}

/*!
 * @brief
 * @param clst Cluster number (must be greater or equal to 2)
 * @param fsp FAT specified data
 * @return Sector number of \p cluster
 */
static uint32_t get_sect_of_clust(uint32_t clst, fat_spec_t *fsp) {
    return ((clst - 2) << fsp->sec_per_clst_log) + fsp->data_sector;
}

/*
#include <stdio.h>

static void dump_sector(uint8_t *buf) {
    for (uint8_t line = 0; line < 32; line++) {
        for (uint8_t col = 0; col < 16; col++) {
            printf_P(PSTR("%02X "), buf[(line << 4) + col]);
        }
        putchar('\n');
    }
    putchar('\n');
}   // */

const struct vol_ops fat_ops PROGMEM = {
    .create = NULL,
    .lookup = NULL,
    .mkdir = NULL,
    .rmdir = NULL,
    .rename = NULL,
    .setattr = NULL,
    .getattr = NULL,
    .update_time = NULL,
};

const struct fat_entry_ops fat16_ops PROGMEM = {

};

const struct fat_entry_ops fat32_ops PROGMEM = {

};

/*
const struct fat_entry_ops fat12_ops PROGMEM = {
};
const struct fat_entry_ops exfat_ops PROGMEM = {
};
*/

/*!
 * @brief Initialize FAT file system
 * @param vol Pointer to volume structure
 * @param req Pointer to request structure
 * @return 0 on success
 */
int8_t fat_init(fs_volume_t *vol, req_t *req) {
    fat_spec_t *fat_spec;
    fat_cache_t *cache;
    req_f req_func;
    int8_t ret = -1;

    uint8_t root_dir_sectors;

    fat_spec = malloc(sizeof(fat_spec_t));

    if (!fat_spec)
        // ENOMEM
        return ret;

    vol->fs_spec = fat_spec;

    cache = req->buf;
    req_func = pgm_read_ptr(&vol->bdev->blk_ops->request);
    req->block = vol->start_sector;

    ret = req_func(req);
    if (ret)
        // some err
        return ret;

    fat_spec->bytes_per_sec_log = log_2(cache->bpb.BPB_BytsPerSec);
    fat_spec->sec_per_clst_log = log_2(cache->bpb.BPB_SecPerClus);
    fat_spec->sec_per_fat = cache->bpb.BPB_FATSz16 ?
                            cache->bpb.BPB_FATSz16 :
                            cache->bpb.fat32_ext.BPB_FATSz32;
    fat_spec->fat_number = cache->bpb.BPB_NumFATs;
    fat_spec->fat_sector = vol->start_sector + cache->bpb.BPB_RsvdSecCnt;

    {   // FAT type determination
        uint32_t tot_sects;
        uint32_t data_sects;

        root_dir_sectors = (((cache->bpb.BPB_RootEntCnt << 5) +
                             (cache->bpb.BPB_BytsPerSec - 1)) >>
                            fat_spec->bytes_per_sec_log);
        tot_sects = cache->bpb.BPB_TotSec16 ?
                    cache->bpb.BPB_TotSec16 :
                    cache->bpb.BPB_TotSec32;
        data_sects = tot_sects - (cache->bpb.BPB_RsvdSecCnt +
                                  (fat_spec->sec_per_fat *
                                   fat_spec->fat_number) +
                                  root_dir_sectors);
        fat_spec->tot_clusters = data_sects >> fat_spec->sec_per_clst_log;

        if (fat_spec->tot_clusters < 4085) {
            fat_spec->fat_type = FAT12; // not supported yet
            // fat_spec->ent_ops = &fat12_ops;
            free(fat_spec);
            return -1;
        } else if (fat_spec->tot_clusters < 65525) {
            fat_spec->fat_type = FAT16;
            fat_spec->ent_ops = &fat16_ops;
        } else if (fat_spec->tot_clusters < 268435445) {
            fat_spec->fat_type = FAT32;
            fat_spec->ent_ops = &fat32_ops;
        } else {
            fat_spec->fat_type = FAT64; // not supported yet
            // fat_spec->ent_ops = &exfat_ops;
            free(fat_spec);
            return -1;
        }
    }

    fat_spec->data_sector = fat_spec->fat_sector + (fat_spec->sec_per_fat * fat_spec->fat_number);

    switch (fat_spec->fat_type) {
        case FAT12:
        case FAT16:
            fat_spec->root_sector = fat_spec->data_sector;
            fat_spec->data_sector = fat_spec->root_sector + root_dir_sectors;
            vol->root.clust = 2;
            break;

        case FAT32:
            fat_spec->root_sector = get_sect_of_clust(cache->bpb.fat32_ext.BPB_RootClus, fat_spec);
            vol->root.clust = cache->bpb.fat32_ext.BPB_RootClus;
            break;

        default:    // unreachable
            break;
    }

    // setting of root directory
    vol->root.vol = vol;
    vol->root.sect = fat_spec->root_sector;
    vol->root.offset = 0;
    vol->root.entry = NULL;
    vol->root.ent_size = 32;

    return ret;
}
