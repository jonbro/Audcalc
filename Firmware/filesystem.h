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

#define BLOCK_SIZE 0x1000

typedef struct
{
    int     (*erase)  (uint32_t offset, size_t size);
    int      (*read)   (uint32_t offset, size_t size, void *dst);
    int      (*write)  (uint32_t offset, size_t size, void *buffer);
    uint32_t offset;
    uint32_t size;
} ffs_cfg;

typedef struct
{
    bool initialized;
    uint16_t object_id;
    uint8_t read_offset;
    uint32_t current_block;
} ffs_file;

typedef struct
{
   int     (*erase)  (uint32_t offset, size_t size);
   int      (*read)   (uint32_t offset, size_t size, void *dst);
   int      (*write)  (uint32_t offset, size_t size, void *buffer);
   uint32_t offset;
   uint32_t size;
   void     *work_buf;
} ffs_filesystem;

typedef struct 
{
    uint16_t object_id;
    uint16_t written_page_mask;
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
static int ffs_mount(ffs_filesystem *fs, const ffs_cfg *cfg, void *work_buf)
{
    // copy cfg into our filesystem
    fs->erase = cfg->erase;
    fs->write = cfg->write;
    fs->read = cfg->read;
    fs->offset = cfg->offset;
    fs->size = cfg->size;
    fs->work_buf = work_buf;
}


static int ffs_open(ffs_filesystem *fs, ffs_file *file, uint16_t file_id)
{
    // cannot use the top bit of the file id, used for marking dead pages
    assert((file_id & 0x8000) == 0);
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
            file->current_block = block_offset;
            file->read_offset = 0;  
            break;
        }
        block_offset += BLOCK_SIZE;
    }
    if(!found)
    {
        file->initialized = false;
    }
    file->object_id = file_id;
}
static int ffs_find_empty(ffs_filesystem *fs)
{
    uint32_t block_offset = 0;
    ffs_blockheader blockHeader;
    while(block_offset < fs->size)
    {
        fs->read(block_offset, sizeof(ffs_blockheader), &blockHeader);
        // find empty block?
        if(blockHeader.object_id == 0xffff)
        {
            return block_offset;
        }
        block_offset += BLOCK_SIZE;
    }
    return -1;
}
static int ffs_append(ffs_filesystem *fs, ffs_file *file, void *buffer, size_t size)
{
    uint32_t block_offset = 0;
    ffs_blockheader blockHeader;
    if(!file->initialized)
    {
        // find a new empty block to write into
        int block_offset = ffs_find_empty(fs);
        if(block_offset < 0)
        {
            return -1;
        }
        blockHeader.object_id = file->object_id;
        blockHeader.initial_page = true;
        blockHeader.jump_page = 0xffffffff;
        blockHeader.written_page_mask = ~1; // mark the first page as filled - this is done in inverted

        // we are wasting a crapton of space here, but its ok?
        // easier just to waste and keep everything aligned rather than doing a bunch of copies at this point

        memset(fs->work_buf, 0xff, 256);
        memcpy(fs->work_buf, &blockHeader, sizeof(ffs_blockheader));
        fs->write(block_offset, 256, fs->work_buf);
        // step forward to the next 256 byte aligned chunks
        block_offset += 256;

        // need to handle going out of the block, but for now we can safely write into this chunk 
        // in fact - if we never write more than 256, we can safely just use this
        memset(fs->work_buf, 0xff, 256);
        memcpy(fs->work_buf, buffer, size);
        fs->write(block_offset, 256, fs->work_buf);
        file->current_block = block_offset-256;
        file->read_offset = 0;
        file->initialized = true;
        return 0;
    }
    else
    {
        while(block_offset < fs->size)
        {
            fs->read(block_offset, sizeof(ffs_blockheader), &blockHeader);
            // look for last block
            if(blockHeader.object_id == file->object_id)
            {
                if(blockHeader.jump_page != 0xffffffff)
                {
                    block_offset = blockHeader.jump_page;
                    continue;
                }
                int foundPage = -1;
                // find the first empty page
                for (size_t i = 0; i < 15; i++)
                {
                    if((~blockHeader.written_page_mask & (1<<i)) == 0)
                    {
                        foundPage = i;
                        break;
                    }
                }
                if(foundPage < 0)
                {
                    // this block has been filled, find a new empty block to write into
                    int empty = ffs_find_empty(fs);
                    if(empty < 0)
                    {
                        return -1;
                    }
                    blockHeader.jump_page = empty;
                    memset(fs->work_buf, 0xff, 256);
                    memcpy(fs->work_buf, &blockHeader, sizeof(ffs_blockheader));
                    fs->write(block_offset, 256, fs->work_buf);

                    // clear block header and write into new empty page
                    blockHeader.jump_page = 0xffffffff;
                    blockHeader.object_id = file->object_id;
                    blockHeader.written_page_mask = 0xffff;

                    memset(fs->work_buf, 0xff, 256);
                    memcpy(fs->work_buf, &blockHeader, sizeof(ffs_blockheader));
                    block_offset = empty;
                    fs->write(block_offset, 256, fs->work_buf);
                    continue;
                }
                //update the pagemask
                blockHeader.written_page_mask = ~(1<<foundPage);
                memset(fs->work_buf, 0xff, 256);
                memcpy(fs->work_buf, &blockHeader, sizeof(ffs_blockheader));
                fs->write(block_offset, 256, fs->work_buf);

                // write the data into the correct page!
                memset(fs->work_buf, 0xff, 256);
                memcpy(fs->work_buf, buffer, 256);
                fs->write(block_offset+(foundPage+1)*256, 256, fs->work_buf);
                return 0;
            }
            block_offset += BLOCK_SIZE;
        }
    }
    return -1;
}

static int ffs_erase(ffs_filesystem *fs, ffs_file *file)
{
    // find any blocks that have this file in them and remove
    uint32_t block_offset = 0;
    ffs_blockheader blockHeader;
    while(block_offset < fs->size)
    {
        fs->read(block_offset, sizeof(ffs_blockheader), &blockHeader);
        // found matching block
        if(blockHeader.object_id == file->object_id)
        {
            fs->erase(block_offset, BLOCK_SIZE);
            // use jumps to erase the rest
            while(blockHeader.jump_page != 0xffffffff)
            {
                block_offset = blockHeader.jump_page;
                fs->read(block_offset, sizeof(ffs_blockheader), &blockHeader);
                fs->erase(block_offset, BLOCK_SIZE);
            }
            file->initialized = false;
            return 0;
        }
        block_offset+=BLOCK_SIZE;
    }
    // couldn't find the file
    return -1;
}

static int ffs_seek(ffs_filesystem *fs, ffs_file *file, size_t position)
{
    
}

static int ffs_read(ffs_filesystem *fs, ffs_file *file, void *buffer, size_t size)
{
    if(!file->initialized)
    {
        return -1; // need better error codes
    }

    // need to actually keep track of read state!
    uint32_t read_offset = file->current_block;
    ffs_blockheader blockHeader;

    fs->read(read_offset, sizeof(blockHeader), &blockHeader);
    assert(blockHeader.object_id == file->object_id);
    // confirm the read offset is readable
    assert((~blockHeader.written_page_mask & (1<<file->read_offset)) != 0);

    read_offset = file->current_block + 256*(file->read_offset+1); // compensate for the block header
    fs->read(read_offset, size, buffer);
    // advance the read stuff
    if(++file->read_offset >= 15)
    {
        // advance to the next block
        file->read_offset = 0;

        //TODO: handle out of bounds here
        // maybe just rewind to the beginning lol.
        file->current_block = blockHeader.jump_page;  
    }
    return 0;
}

int file_read(uint32_t offset, size_t size, void *buffer);
int file_write(uint32_t offset, size_t size, void *buffer);
int file_erase(uint32_t offset, size_t size);

// spiffs handlers

void my_spiffs_mount();

void TestFS();

lfs_t* GetLFS();

#ifdef __cplusplus
}
#endif

#endif // filesystem_H_