#define FFS_IMPLEMENTATION
#include "filesystem.h"

// arbitrary offset into flash, hopefully doesn't overlap with the program space lol
#define FS_START 0x40000
const uint8_t *flash_start = (const uint8_t *) (XIP_BASE + FS_START);

int file_read(uint32_t offset, size_t size, void *buffer)
{
    memcpy(buffer, flash_start+offset, size);
    return 0;
}
int __not_in_flash_func(file_write)(uint32_t offset, size_t size, void *buffer)
{
    multicore_lockout_start_blocking();
    uint32_t ints = save_and_disable_interrupts();
    flash_range_program(FS_START + offset, buffer, size);
    restore_interrupts(ints);
        multicore_lockout_end_blocking();
    return 0;
}

int __not_in_flash_func(file_erase)(uint32_t offset, size_t size)
{
    // flash erase automatically switches between sector & block erase depending on the address alignment and requested erase size
    // see https://github.com/raspberrypi/pico-bootrom/blob/master/bootrom/program_flash_generic.c#L333
    //printf("ERASE: %p, %d\n", FS_START + offset, size);
    multicore_lockout_start_blocking();
    uint32_t ints = save_and_disable_interrupts();
    // // re-enable the audio related interrupts so we don't mess up the dac
    irq_set_enabled(DMA_IRQ_0, true);
    irq_set_enabled(DMA_IRQ_1, true);
    flash_range_erase(FS_START + offset,size);
    restore_interrupts(ints);
    multicore_lockout_end_blocking();
    return 0;
}
ffs_filesystem filesystem;
uint8_t fs_work_buf[256];

ffs_filesystem* GetFilesystem()
{
    return &filesystem;
}

void InitializeFilesystem(bool fullClear)
{
    // file system should be sound, don't need to erase
    if(fullClear)
        file_erase(0, 16*1024*1024-0x40000);
    ffs_cfg cfg = {
        .erase = file_erase,
        .read = file_read,
        .write = file_write,
        .size = 16*1024*1024-0x40000
    };
    ffs_mount(&filesystem, &cfg, fs_work_buf);

    // open the file, delete and move on
    // for (size_t i = 0; i < 15; i++)
    // {
    //     ffs_file f0;
    //     ffs_open(&filesystem, &f0, i);
    //     ffs_erase(&filesystem, &f0);
    // }
}
bool ConfirmErasedPage(int offset)
{
    for(int j=0;j<0x1000;j++)
    {
        uint8_t buffer;
        file_read(j+offset, 1, &buffer);
        if(buffer != 0xff)
        {
            printf("page erase failed\n");
            return false;
        }
    }
    return true;
}

bool DeleteNonEmpty(ffs_filesystemClearState *state)
{
    uint8_t buffer;
    bool res = false;
    for(int j=0;j<0x1000;j++)
    {
        file_read(state->offset+j, 1, &buffer);
        if(buffer != 0xff)
        {
            // need to erase this sector
            file_erase(state->offset, 0x1000);
            state->error = !ConfirmErasedPage(state->offset);
            break;
        }
    }
    state->offset += 0x1000;
    state->complete = state->offset >= (16*1024*1024-0x40000);
    return !state->complete;
}

void TestFileSystem()
{
    printf("starting filesystem test\n");

    if(0)
    {
        absolute_time_t startTime = get_absolute_time();
        file_erase(0, 16*1024*1024-0x40000);
        printf("full erase timing ms: %lld\n", absolute_time_diff_us(startTime, get_absolute_time())/1000);
    }

    {
        // write a single sector, erase, and time that
        uint8_t worstCaseSector[4096] = {1};
        file_write(0, 4096, worstCaseSector);
        absolute_time_t startTime = get_absolute_time();
        file_erase(0, 4096);
        printf("single sector erase us: %lld\n", absolute_time_diff_us(startTime, get_absolute_time()));
    }

    // clean up enough space for the test
    {
        absolute_time_t startTime = get_absolute_time();
        file_erase(0, 0x1000*32);
        printf("32 sectors erase us: %lld\n", absolute_time_diff_us(startTime, get_absolute_time()));
    }

    // lets just confirm that reading works the way we expect
    ffs_blockheader header;
    file_read(0, sizeof(ffs_blockheader), &header);
    printf("headerobjectid: %x\n", header.object_id);

    ffs_cfg cfg = {
        .erase = file_erase,
        .read = file_read,
        .write = file_write,
        .size = 0x1000*32
    };
    ffs_filesystem fs;
    ffs_file file0;
    ffs_mount(&fs, &cfg, fs_work_buf);
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

    {
        // file size must be a multiple of 256 / pagesize
        uint32_t filesize = ffs_file_size(&fs, &file0);
        printf("filesize: %i\n", filesize);
        assert(filesize == 256);
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
    {
        absolute_time_t startTime = get_absolute_time();
        // erase file
        if(ffs_erase(&fs, &file0))
        {
            printf("error in erase 1\n");
        }
        printf("file erase time: %lld\n", absolute_time_diff_us(startTime, get_absolute_time()));
    }
    
    // confirm file erased
    {
        uint8_t output[256];
        // lets check the first few blocks - things shouldn't have gone out of bounds here
        for (size_t j = 0; j < 32; j++)
        {
            // direct file read, since we don't have any protection
            file_read(j*256, 256, output);
            for (size_t i = 0; i < 256; i++)
            {
                assert(output[i] == 0xff);
            }
        }
    }

    // simple inblock seeks
    {
        uint16_t input[128];
        uint16_t counter = 0;
        uint16_t output;
        // load two blocks
        for (size_t i = 0; i < 2; i++)
        {
            for (size_t j = 0; j < 128; j++)
            {
                input[j] = counter++;
            }
            ffs_append(&fs, &file0, input, 256);
        }
        
        // read some offsets
        ffs_seek(&fs, &file0, 0);
        ffs_read(&fs, &file0, &output, 2);
        assert(output == 0);

        ffs_seek(&fs, &file0, 10);
        ffs_read(&fs, &file0, &output, 2);
        assert(output == 5);

        // todo, handle reverse seeking
    }

    if(ffs_erase(&fs, &file0))
    {
        printf("error in erase 2\n");
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
            }
        }
        // check rational first page
        ffs_seek(&fs, &file0, 0);
        ffs_read(&fs, &file0, input, 10); // need to read double because of 16bit :D
        for (size_t j = 0; j < 5; j++)
        {
            assert(input[j] == j);
        }

        // check sub page reads
        for (size_t i = 0; i < 128*2; i+=5) // read 5 at a time, so that we get cross page?
        {
            ffs_seek(&fs, &file0, i*2);
            ffs_read(&fs, &file0, input, 10);
            for (size_t j = 0; j < 5; j++)
            {
                assert(input[j] == (i+j)%4096);
            }
        }
        
        // check cross block reads
        {
            int afterblock = 256*15; // this should be after the last page in the first block
            ffs_seek(&fs, &file0, afterblock-2); // back it up to the last value in the first block
            // read 2 uint16s
            ffs_read(&fs, &file0, input, 4);
            assert(input[0] == 128*15-1 && input[1] == 128*15);
        }
    }

    {
        // initial open filesize lookup
        ffs_open(&fs, &file0, 0);
        printf("file size%i\n", ffs_file_size(&fs, &file0));
    }
    printf("test success\n");
}