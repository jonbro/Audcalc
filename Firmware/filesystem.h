#ifndef filesystem_H_
#define filesystem_H_

// was getting an assert failure in fopen
#define LFS_NO_ASSERT 0

#include "littlefs/lfs.h"
#include "hardware/flash.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "pico/multicore.h"
#include <stdio.h>
#include <string.h>
#include "AudioSampleSine440.h"
#include <spiffs/spiffs.h>

#ifdef __cplusplus
extern "C" {
#endif

/*

very limited file system, but I have different requirements:
- no erasing in the hotloop
- append only files
- guarenteed write size of 256 bytes

*/

#define BLOCK_SIZE 0xfff

typedef struct
{
    int      (*write)  (void *buffer, uint32_t offset, size_t size);
    void     (*erase)  (uint32_t offset, size_t size);
    int      (*read)   (uint32_t offset, size_t size, void *dst);
    uint32_t offset;
    uint32_t size;

} ffs_cfg;

typedef struct
{

} ffs_file;

typedef struct
{
   int      (*write)  (void *buffer, uint32_t offset, size_t size);
   void     (*erase)  (uint32_t offset, size_t size);
   int      (*read)   (uint32_t  offset, size_t size, void *dst);
   uint32_t offset;
   uint32_t size;
} ffs_filesystem;

typedef struct 
{
    uint16_t object_id;
    uint32_t jump_page;
    uint8_t  initial_page;
} ffs_blockheader;


/* each block contains

block header
------------
object id - if 0xff then free.
jump page - offset of the next page that contains data for this object (0xff if the object ends within this page)
active page bitmask
initial page - 0x01

*/

// these should return some kind of error code on failure
// no formatting, we assume things have been cleaned up for us in advance of writing
static int ffs_mount(ffs_filesystem *fs, const ffs_cfg *cfg)
{
    // copy cfg into our filesystem
    fs->erase = cfg->erase;
    fs->write = cfg->write;
    fs->read = cfg->read;
    fs->offset = cfg->offset;
    fs->size = cfg->size;
}


static int ffs_open(ffs_filesystem *fs, ffs_file *file, uint16_t file_id)
{
    // cannot use the top bit of the file id, used for marking dead pages
    assert(file_id & 0x8000 == 0);
    // search the blocks for the start block that contains this id
    bool found = false;
    uint32_t block_offset = 0;
    ffs_blockheader blockHeader;
    while(block_offset < fs->size)
    {
        fs->read(block_offset, 1, &blockHeader);
        if(blockHeader.object_id == file_id)
        {
            found = true;
            break;
        }
        block_offset += BLOCK_SIZE;
    }
}

static int ffs_append(ffs_filesystem *fs, ffs_file *file, void *buffer, size_t size)
{
    
}

static int ffs_erase(ffs_filesystem *fs, ffs_file *file)
{
    
}

static int ffs_seek(ffs_filesystem *fs, ffs_file *file, size_t position)
{
    
}

static int ffs_read(ffs_filesystem *fs, ffs_file *file, void *buffer, size_t size)
{

}

static int lfs_flash_read(const struct lfs_config *c, lfs_block_t block,
                            lfs_off_t off, void *buffer, lfs_size_t size);
static int lfs_flash_prog(const struct lfs_config *c, lfs_block_t block,
                            lfs_off_t off, const void *buffer, lfs_size_t size);
static int lfs_flash_erase(const struct lfs_config *c, lfs_block_t block);
static int lfs_flash_sync(const struct lfs_config *c);

int file_read(void *buffer, uint32_t offset, size_t size);
int file_write(void *buffer, uint32_t offset, size_t size);
int file_erase();

// spiffs handlers
static s32_t my_spiffs_read(u32_t addr, u32_t size, u8_t *dst);
static s32_t my_spiffs_write(u32_t addr, u32_t size, u8_t *src);
static s32_t my_spiffs_erase(u32_t addr, u32_t size);

void my_spiffs_mount();

void TestFS();

lfs_t* GetLFS();

#ifdef __cplusplus
}
#endif

#endif // filesystem_H_