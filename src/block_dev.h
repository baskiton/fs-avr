#ifndef BLOCK_DEVICE_H
#define BLOCK_DEVICE_H

#include <stdint.h>

typedef struct block_dev_s {
    uint16_t bd_blk_size;   // size of block (512 bytes by default)
    uint32_t bd_blk_num;    // number of blocks
    const struct blk_dev_ops_s *blk_ops;  // block device operations
    void *priv;             // private data
} bdev_t;

typedef struct request_s {
    bdev_t *bdev;
    uint8_t cmd_flags;
#define REQ_READ 0
#define REQ_WRITE 1
    uint32_t block;     // start block in LBA
    void *buf;          // src or dst
    // uint16_t offset;    // Offset from start of block in bytes; only for read
    // uint16_t count;     // Number of bytes to read; 512 max; only for read
} req_t;

typedef int8_t (*req_f)(req_t *);

struct blk_dev_ops_s {
    // int8_t (*open)(bdev_t *, uint8_t);
    // void (*release)(bdev_t *, uint8_t);
    // int8_t (*request)(struct request_s *);
    req_f request;
};

static inline void blk_set_priv(bdev_t *bdev, void *priv) {
    bdev->priv = priv;
}

static inline void *blk_get_priv(bdev_t *bdev) {
    return bdev->priv;
}

static inline void blk_set_dev(spi_dev_t *dev, bdev_t *bdev) {
    spi_set_priv(dev, bdev);
}

static inline bdev_t *blk_get_dev(spi_dev_t *dev) {
    return (bdev_t *)spi_get_priv(dev);
}

#endif  /* !BLOCK_DEVICE_H */
