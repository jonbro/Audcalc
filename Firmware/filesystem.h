#ifndef filesystem_H_
#define filesystem_H_

#include "hardware/flash.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "pico/multicore.h"
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
#include <cassert>
extern "C" {
#else
#include <assert.h>
#endif

/*

very limited file system, but I have different requirements:
- no erasing in the hotloop
- append only files
- guarenteed write size of 256 bytes
- REQUIRES WRITE SIZE OF 256!!!

*/

#define BLOCK_SIZE 0x1000
#define EMPTY_BLOCK 0xffffffff

typedef struct
{
    int     (*erase)  (uint32_t offset, size_t size);
    int      (*read)   (uint32_t offset, size_t size, void *dst);
    int      (*write)  (uint32_t offset, size_t size, void *buffer);
    uint32_t offset;
    uint32_t size;
    uint32_t empty_search_offset; // used to describe where searching for empty blocks starts, provide a random number to get some amount of flash leveling
} ffs_cfg;

typedef struct
{
    bool        initialized;
    uint16_t    object_id;
    
    uint32_t    filesize;
    uint32_t    logical_read_offset; // where we are in the file (disconnected from physical address)
    uint8_t     inblock_read_offset;
    uint32_t    current_block;
} ffs_file;

typedef struct
{
   int     (*erase)  (uint32_t offset, size_t size);
   int      (*read)   (uint32_t offset, size_t size, void *dst);
   int      (*write)  (uint32_t offset, size_t size, void *buffer);
   uint32_t offset;
   uint32_t size;
   void     *work_buf;
   uint32_t empty_search_offset; // used to describe where searching for empty blocks starts, provide a random number to get some amount of flash leveling
} ffs_filesystem;

typedef struct 
{
    uint16_t    object_id;
    uint16_t    written_page_mask;
    uint32_t    next_block; // should rename to "next block"
    uint32_t    prior_block;
    uint8_t     initial_page;
} ffs_blockheader;


/* each block contains

block header
------------
object id - if 0xff then free.
jump page - offset of the next page that contains data for this object (0xff if the object ends within this page)
active page bitmask
initial page - 0x01

*/

#ifdef FFS_STATIC
#define FFS_DEF static
#else
#define FFS_DEF extern
#endif

FFS_DEF int ffs_mount(ffs_filesystem *fs, const ffs_cfg *cfg, void *work_buf);
FFS_DEF int ffs_open(ffs_filesystem *fs, ffs_file *file, uint16_t file_id);
FFS_DEF int ffs_append(ffs_filesystem *fs, ffs_file *file, void *buffer, size_t size);
FFS_DEF int ffs_seek(ffs_filesystem *fs, ffs_file *file, size_t position);
FFS_DEF int ffs_read(ffs_filesystem *fs, ffs_file *file, void *buffer, size_t size);
FFS_DEF int ffs_erase(ffs_filesystem *fs, ffs_file *file);
FFS_DEF int ffs_file_size(ffs_filesystem *fs, ffs_file *file);

#ifdef FFS_IMPLEMENTATION
// these should return some kind of error code on failure
// no formatting, we assume things have been cleaned up for us in advance of writing
FFS_DEF int ffs_mount(ffs_filesystem *fs, const ffs_cfg *cfg, void *work_buf)
{
    // empty search offset needs to be aligned to the block size
    uint32_t empty_search_offset_aligned = cfg->empty_search_offset+(BLOCK_SIZE-cfg->empty_search_offset%BLOCK_SIZE);
    empty_search_offset_aligned = empty_search_offset_aligned%cfg->size;
    // copy cfg into our filesystem
    fs->erase = cfg->erase;
    fs->write = cfg->write;
    fs->read = cfg->read;
    fs->offset = cfg->offset;
    fs->size = cfg->size;
    fs->empty_search_offset = empty_search_offset_aligned;
    fs->work_buf = work_buf;
}

static int16_t ffs_find_empty_page(ffs_blockheader *blockheader)
{
    int foundPage = -1;
    // find the first empty page
    for (size_t i = 0; i < 15; i++)
    {
        if((~blockheader->written_page_mask & (1<<i)) == 0)
        {
            foundPage = i;
            break;
        }
    }
    return foundPage;
}

inline FFS_DEF uint32_t ffs_file_current_block_logical_start(ffs_filesystem *fs, ffs_file *file)
{
    return file->logical_read_offset-(file->logical_read_offset%(256*15));
}

FFS_DEF int ffs_open(ffs_filesystem *fs, ffs_file *file, uint16_t file_id)
{
    // cannot use the top bit of the file id, used for marking dead pages
    assert((file_id & 0x8000) == 0);
    // search the blocks for the start block that contains this id
    bool found = false;
    uint32_t block_offset = 0;
    ffs_blockheader blockHeader;
    while(block_offset < fs->size)
    {
        fs->read(block_offset, sizeof(ffs_blockheader), &blockHeader);
        if(blockHeader.object_id == file_id)
        {
            found = true;
            file->current_block         = block_offset;
            file->inblock_read_offset   = 0;
            file->logical_read_offset   = 0; 
            // need to walk the filetree to generate the filesize
            file->filesize              = 0;
            file->initialized           = true;
            int writtenPages = ffs_find_empty_page(&blockHeader);
            if(writtenPages >= 0)
            {
                file->filesize+=writtenPages*256;
            }
            else
            {
                file->filesize+=15*256;
            }
            while(blockHeader.next_block != EMPTY_BLOCK)
            {
                fs->read(blockHeader.next_block, sizeof(ffs_blockheader), &blockHeader);
                int writtenPages = ffs_find_empty_page(&blockHeader);
                if(writtenPages >= 0)
                {
                    file->filesize+=writtenPages*256;
                }
                else
                {
                    file->filesize+=15*256;
                }
            }
            break;
        }
        block_offset += BLOCK_SIZE;
    }
    if(!found)
    {
        file->initialized = false;
        file->filesize = 0;
    }
    file->object_id = file_id;
    return 0;
}

static int ffs_find_empty_block(ffs_filesystem *fs, uint32_t search_start, uint32_t fssize)
{
    uint32_t block_offset = 0;
    ffs_blockheader blockHeader;
    uint32_t readPos = fs->empty_search_offset; // this will be set to be lower than fssize at init time
    while(block_offset < fssize)
    {
        fs->read(readPos, sizeof(ffs_blockheader), &blockHeader);
        // find empty block?
        if(blockHeader.object_id == 0xffff)
        {
            return readPos;
        }
        block_offset += BLOCK_SIZE;
        readPos = readPos+BLOCK_SIZE;
        if(readPos>fssize)
        {
            readPos-=fssize;
        }
    }
    return -1;
}

FFS_DEF int ffs_append(ffs_filesystem *fs, ffs_file *file, void *buffer, size_t size)
{
    uint32_t block_offset = 0;
    ffs_blockheader blockHeader;
    if(!file->initialized)
    {
        // find a new empty block to write into
        int block_offset = ffs_find_empty_block(fs, 0, fs->size);
        if(block_offset < 0)
        {
            return -1;
        }
        blockHeader.object_id = file->object_id;
        blockHeader.initial_page = true;
        blockHeader.next_block = EMPTY_BLOCK;
        blockHeader.prior_block = EMPTY_BLOCK;
        blockHeader.written_page_mask = ~1; // mark the first page as filled - this is done in inverted
        //blockHeader.block_logical_start = 0;
        // we are wasting a crapton of space here, but its ok?
        // easier just to waste and keep everything aligned rather than doing a bunch of copies at this point

        memset(fs->work_buf, 0xff, 256);
        memcpy(fs->work_buf, &blockHeader, sizeof(ffs_blockheader));
        fs->write(block_offset, 256, fs->work_buf);
        // step forward to the next 256 byte aligned chunks

        // need to handle going out of the block, but for now we can safely write into this chunk 
        // in fact - if we never write more than 256, we can safely just use this
        memset(fs->work_buf, 0xff, 256);
        memcpy(fs->work_buf, buffer, size);
        fs->write(block_offset+256, 256, fs->work_buf);
        file->current_block = block_offset;
        file->inblock_read_offset = 0;
        file->logical_read_offset = 0;
        file->initialized = true;
        file->filesize+=256;
        return 0;
    }
    else
    {
        // faster writes if we store the blockwrite position
        while(block_offset < fs->size)
        {
            fs->read(block_offset, sizeof(ffs_blockheader), &blockHeader);
            // look for last block
            if(blockHeader.object_id == file->object_id)
            {
                if(blockHeader.next_block != 0xffffffff)
                {
                    block_offset = blockHeader.next_block;
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
                    int empty = ffs_find_empty_block(fs, 0, fs->size);
                    if(empty < 0)
                    {
                        return -1;
                    }
                    blockHeader.next_block = empty;
                    memset(fs->work_buf, 0xff, 256);
                    memcpy(fs->work_buf, &blockHeader, sizeof(ffs_blockheader));
                    fs->write(block_offset, 256, fs->work_buf);

                    //uint32_t last_logical_start = blockHeader.block_logical_start;
                    // clear block header and write into new empty page
                    blockHeader.next_block = EMPTY_BLOCK;
                    blockHeader.object_id = file->object_id;
                    blockHeader.written_page_mask = 0xffff;
                    //blockHeader.block_logical_start = last_logical_start+15*256;
                    blockHeader.prior_block = block_offset;
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
                file->filesize+=256;
                return 0;
            }
            block_offset += BLOCK_SIZE;
        }
    }
    return -1;
}

FFS_DEF int __not_in_flash_func(ffs_erase)(ffs_filesystem *fs, ffs_file *file)
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
            while(blockHeader.next_block != EMPTY_BLOCK)
            {
                block_offset = blockHeader.next_block;
                fs->read(block_offset, sizeof(ffs_blockheader), &blockHeader);
                fs->erase(block_offset, BLOCK_SIZE);
            }

            file->initialized = false;
            file->inblock_read_offset   = 0;
            file->logical_read_offset   = 0; 
            file->filesize              = 0;
            return 0;
        }
        block_offset+=BLOCK_SIZE;
    }
    // couldn't find the file
    // lets just make sure its all unitialized anyways
    file->initialized = false;
    file->inblock_read_offset   = 0;
    file->logical_read_offset   = 0; 
    file->filesize              = 0;
    return -1;
}

static int ffs_load_blockheader(ffs_filesystem *fs, ffs_file *file, ffs_blockheader *blockheader)
{
    ffs_blockheader headerForSize;
    fs->read(file->current_block, sizeof(headerForSize), blockheader);
}

inline FFS_DEF int ffs_seek(ffs_filesystem *fs, ffs_file *file, size_t position)
{
    // early out if logical read position already set correctly
    if(file->logical_read_offset == position)
    {
        return 0;
    }
    // early out if read position is past end of file
    if(position >= file->filesize)
    {
        return -1;
    }
    // load blockheader
    ffs_blockheader blockHeader;

    fs->read(file->current_block, sizeof(blockHeader), &blockHeader);
    uint32_t current_block_logical_start = ffs_file_current_block_logical_start(fs, file);
    if(position >= current_block_logical_start)
    {
        // seeking forward
        if(position-current_block_logical_start < 256*15)
        {
            // if the read position is within the current block, we can just move things forward
            file->logical_read_offset = position;
            return 0;
        }
        else
        {
            // we are going forward to a later block
            while(blockHeader.next_block != EMPTY_BLOCK)
            {
                file->current_block = blockHeader.next_block;
                fs->read(file->current_block, sizeof(blockHeader), &blockHeader);
                current_block_logical_start += 256*15;
                if(position-current_block_logical_start < 256*15)
                {
                    // if the read position is within the current block, we can just move things forward
                    file->logical_read_offset = position;
                    return 0;
                }
            }
            // we shouldn't hit this, it should have caught this error because we are asking for something
            // past the end of the file
            assert(false);
        }
    }
    else
    {
        // seeking backwards
        while(blockHeader.prior_block != EMPTY_BLOCK)
        {
            file->current_block = blockHeader.prior_block;
            fs->read(file->current_block, sizeof(blockHeader), &blockHeader);
            current_block_logical_start -= 256*15;
            if(position-current_block_logical_start < 256*15)
            {
                // if the read position is within the current block, we can just move things forward
                file->logical_read_offset = position;
                return 0;
            }
        }
        // we shouldn't hit this, it should have caught this error because we are asking for something
        // past the beginning of the file. Should be impossible to even do this, since we are using uints
        // anyways, asserts are for logical errors
        assert(false);
    }
    
}


inline FFS_DEF int ffs_file_size(ffs_filesystem *fs, ffs_file *file)
{
    return file->filesize;
}

FFS_DEF int ffs_read(ffs_filesystem *fs, ffs_file *file, void *buffer, size_t size)
{
    if(!file->initialized)
    {
        // file isn't initialized?
        return -1; // need better error codes
    }

    // need to actually keep track of read state!
    ffs_blockheader blockHeader;

    fs->read(file->current_block, sizeof(blockHeader), &blockHeader);
    assert(blockHeader.object_id == file->object_id);
    assert(file->logical_read_offset >= blockHeader.block_logical_start);

    int read_position = file->logical_read_offset-ffs_file_current_block_logical_start(fs, file);    
    // clamp the read amount to the remaining in the block
    int next_read_amount = (read_position+size>256*15)?256*15-read_position:size;
    //printf("position: %i %i\n", read_position, next_read_amount);
    fs->read(read_position+file->current_block+256, next_read_amount, buffer);
    if(size-next_read_amount>0)
    {
        // this will advance the read to the next block
        ffs_seek(fs, file, next_read_amount+file->logical_read_offset);
        // kinda dangerous to recurse, but lets assume we aren't gonna overflow the stack, lol.
        return ffs_read(fs, file, ((uint8_t*)buffer)+next_read_amount, size-next_read_amount);
    }
    // advance the read position
    // ignore the boundary error at this point, even if it is out of bounds
    // the next file read will trigger the boundary error?
    ffs_seek(fs, file, file->logical_read_offset+size);
    return 0;
}
#endif //FFS_IMPLEMENTATION



int file_read(uint32_t offset, size_t size, void *buffer);
int file_write(uint32_t offset, size_t size, void *buffer);
int file_erase(uint32_t offset, size_t size);

void TestFileSystem();
void InitializeFilesystem(bool fullClear, uint32_t empty_search_offset);

typedef struct 
{
    bool complete;
    int offset;
    int error;
} ffs_filesystemClearState;

#define ffs_filesystemClearState_init               {0, 0, 0}

bool DeleteNonEmpty(ffs_filesystemClearState *state);

ffs_filesystem* GetFilesystem();

#ifdef __cplusplus
}
#endif

#endif // filesystem_H_