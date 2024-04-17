#ifndef filesystem_H_
#define filesystem_H_

#include "littlefs/lfs.h"
#include "hardware/flash.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "pico/multicore.h"
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

void InitFilesystem();
lfs_t* GetFilesystem();
//FLASH_PAGE_SIZE
int file_read(const struct lfs_config *c,
                                 lfs_block_t block, lfs_off_t off, void *dst, lfs_size_t size);

int file_write(const struct lfs_config *c, lfs_block_t block,
                    lfs_off_t off, const void *buffer, lfs_size_t size);
int file_erase(const struct lfs_config *c, lfs_block_t block);
int file_sync(const struct lfs_config *c);

#ifdef __cplusplus
}
#endif

#endif // filesystem_H_