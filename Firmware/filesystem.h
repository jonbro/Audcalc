#ifndef filesystem_H_
#define filesystem_H_

// was getting an assert failure in fopen
#define LFS_NO_ASSERT 0

#include "littlefs/lfs.h"
#include "hardware/flash.h"
#include "pico/multicore.h"
#include <stdio.h>
#include <string.h>
#include "AudioSampleSine440.h"

#ifdef __cplusplus
extern "C" {
#endif


static int lfs_flash_read(const struct lfs_config *c, lfs_block_t block,
                            lfs_off_t off, void *buffer, lfs_size_t size);
static int lfs_flash_prog(const struct lfs_config *c, lfs_block_t block,
                            lfs_off_t off, const void *buffer, lfs_size_t size);
static int lfs_flash_erase(const struct lfs_config *c, lfs_block_t block);
static int lfs_flash_sync(const struct lfs_config *c);

void TestFS();

lfs_t* GetLFS();

#ifdef __cplusplus
}
#endif

#endif // filesystem_H_