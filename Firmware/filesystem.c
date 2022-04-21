#include "filesystem.h"

// arbitrary offset into flash, hopefully doesn't overlap with the program space lol
#define FS_START 0xAC000
const uint8_t *flash_start = (const uint8_t *) (XIP_BASE + FS_START);

int file_read(uint32_t offset, size_t size, void *buffer)
{
    memcpy(buffer, flash_start+offset, size);
    return 0;
}
int file_write(uint32_t offset, size_t size, void *buffer)
{
    uint32_t ints = save_and_disable_interrupts();
    flash_range_program(FS_START + offset, buffer, size);
    restore_interrupts(ints);
    return 0;
}

int file_erase(uint32_t offset, size_t size)
{
    //printf("ERASE: %p, %d\n", (intptr_t)addr - (intptr_t)XIP_BASE, c->block_size);
    // multicore_lockout_start_timeout_us(500);
    uint32_t ints = save_and_disable_interrupts();
    // // re-enable the audio related interrupts so we don't mess up the dac
    // irq_set_enabled(DMA_IRQ_0, true);
    // irq_set_enabled(DMA_IRQ_1, true);
    flash_range_erase(FS_START + offset,size);
    restore_interrupts(ints);
    // multicore_lockout_end_timeout_us(500);
    return 0;
}

void TestFS()
{
    uint8_t work_buf[256];
    file_erase(0, 4096*32);
    // lets just confirm that reading works the way we expect
    ffs_blockheader header;
    file_read(0, sizeof(ffs_blockheader), &header);
    printf("headerobjectid: %x\n", header.object_id);

    ffs_cfg cfg = {
        .erase = file_erase,
        .read = file_read,
        .write = file_write,
        .size = 4096*32
    };
    ffs_filesystem fs;
    ffs_file file0;
    ffs_mount(&fs, &cfg, work_buf);
    if(ffs_open(&fs, &file0, 0))
    {
        printf("error in open\n");
    }
    char test[] = "Hello World";
    printf("expecting output %s\n", test);
    if(ffs_append(&fs, &file0, &test, 12))
    {
        printf("error in append\n");
    }
    
    // clear the test string
    memset(&test, 0, 12);
    if(ffs_read(&fs, &file0, &test, 12))
    {
        printf("error in read\n");
    }
    char compare[] = "Hello World"; 
    for(int i=0;i<12;i++)
    {
        assert(compare[i] == test[i]);
    }
    
    // erase file
    if(ffs_erase(&fs, &file0))
    {
        printf("error in erase\n");
    }
    
    // confirm file erased
    {
        uint8_t output[256*2];

        // direct file read, since we don't have any protection
        file_read(0, 256*2, output);
        for (size_t i = 0; i < 256*2; i++)
        {
            assert(output[i] == 0xff);
        }
    }

    // cross block writing
    {
        uint16_t input[128];
        uint16_t counter = 0;
        for (size_t i = 0; i < 20; i++)
        {
            for (size_t j = 0; j < 128; j++)
            {
                input[j] = counter++;
                if(counter > 4096) counter = 0;
            }
            ffs_append(&fs, &file0, input, 256);
        }
        // reset the counter!
        counter = 0;
        for (size_t i = 0; i < 20; i++)
        {
            ffs_read(&fs, &file0, input, 256);
            for (size_t j = 0; j < 128; j++)
            {
                assert(input[j] == counter++);
                if(counter > 4096) counter = 0;
            }
        }
    }

    printf("test success\n");
}
