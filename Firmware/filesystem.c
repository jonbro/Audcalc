#include "filesystem.h"

#define FS_START 0x60000
const char* flash_start = (const char*) (XIP_BASE + FS_START);

// variables used by the filesystem
lfs_t lfs;
lfs_file_t file;

    extern char __flash_binary_end;

void InitFilesystem()
{

    uintptr_t end = (uintptr_t) &__flash_binary_end;
    printf("Binary ends at %08x\n", end);
    // for(int i=0;i<128;i++)
    // {
    //     flash_range_erase(FS_START + i*FLASH_SECTOR_SIZE, FLASH_SECTOR_SIZE);
    // }
    // configuration of the filesystem is provided by this struct
    const struct lfs_config cfg = {
        // block device operations
        .read  = file_read,
        .prog  = file_write,
        .erase = file_erase,
        .sync  = file_sync,

        // block device configuration
        .read_size = 16,
        .prog_size = FLASH_PAGE_SIZE,
        .block_size = FLASH_SECTOR_SIZE,
        .block_count = 128,
        .cache_size = FLASH_PAGE_SIZE,
        .lookahead_size = 16,
        .block_cycles = 500,
    };
    printf("hello?\n");
    // mount the filesystem
    int err = lfs_mount(&lfs, &cfg);
    printf("after mount?\n");

    // reformat if we can't mount the filesystem
    // this should only happen on the first boot
    if (err) {
        printf("formatting\n");
        lfs_format(&lfs, &cfg);
        lfs_mount(&lfs, &cfg);
    }
    printf("format complete\n");
    // read current count
    uint32_t boot_count = 0;
    lfs_file_open(&lfs, &file, "boot_count", LFS_O_RDWR | LFS_O_CREAT);
    lfs_file_read(&lfs, &file, &boot_count, sizeof(boot_count));

    // update boot count
    boot_count += 1;
    lfs_file_rewind(&lfs, &file);
    lfs_file_write(&lfs, &file, &boot_count, sizeof(boot_count));

    // remember the storage is not updated until the file is closed successfully
    lfs_file_close(&lfs, &file);

    // release any resources we were using
    lfs_unmount(&lfs);

    // print the boot count
    printf("boot_count: %d\n", boot_count);
}

lfs_t* GetFilesystem()
{
    return &lfs;
}
//FLASH_PAGE_SIZE
int file_read(const struct lfs_config *c, lfs_block_t block,
            lfs_off_t off, void *buffer, lfs_size_t size)
{
    memcpy(buffer, flash_start + (block * FLASH_SECTOR_SIZE) + off, size);
    return LFS_ERR_OK;
}

int __not_in_flash_func(file_write)(const struct lfs_config *c, lfs_block_t block,
                    lfs_off_t off, const void *buffer, lfs_size_t size)
{
    // multicore_lockout_start_blocking();
    uint32_t ints = save_and_disable_interrupts();
    flash_range_program(FS_START + block*FLASH_SECTOR_SIZE + off, buffer, size);
    restore_interrupts(ints);
    // multicore_lockout_end_blocking();
    printf("write complete\n");
    // write check
    char charArray[size];
    file_read(c, block, off, charArray, size);
    for(int i=0;i<size; i++){
        printf("%c, %c\n", charArray[i], ((char*)buffer)[i]);
    }
    return LFS_ERR_OK;
}

int __not_in_flash_func(file_erase)(const struct lfs_config *c, lfs_block_t block)
{
        printf("erasing %i\n", block);

    // flash erase automatically switches between sector & block erase depending on the address alignment and requested erase size
    // see https://github.com/raspberrypi/pico-bootrom/blob/master/bootrom/program_flash_generic.c#L333
    // multicore_lockout_start_blocking();
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(FS_START + block*FLASH_SECTOR_SIZE, FLASH_SECTOR_SIZE);
    restore_interrupts(ints);
    // multicore_lockout_end_blocking();
    return LFS_ERR_OK;
}


int file_sync(const struct lfs_config *c) {
    return LFS_ERR_OK;
}