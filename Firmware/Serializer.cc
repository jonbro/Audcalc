#include "Serializer.h"

#define flashOffset 0x9F000
const uint8_t *flash_target_contents = (const uint8_t *) (XIP_BASE + flashOffset);
extern uint32_t __flash_binary_end;

void Serializer::Init()
{
    writePosition = 0;
    flashPosition = 0;
    readPosition = 0;
    // FlushToFlash();
}
uint8_t Serializer::GetNextValue()
{
    return flash_target_contents[readPosition++];
}
void Serializer::AddData(uint8_t val)
{
    // TODO: this could save 4k memory if it wrote as it went, rather than keeping
    // the data variable.
    data[writePosition++] = val;
    if(writePosition>FLASH_SECTOR_SIZE)
    {
        FlushToFlash();
    }
}

void Serializer::Finish() 
{
    FlushToFlash();
}

void Serializer::Erase()
{ 
    printf("about to erase\n");
    multicore_lockout_start_timeout_us(500);
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(flashOffset+flashPosition*FLASH_SECTOR_SIZE, FLASH_SECTOR_SIZE);
    restore_interrupts(ints);
    multicore_lockout_end_timeout_us(500);
}

void Serializer::FlushToFlash()
{
    // would probably be better to write as we add data, since we can write in smaller chunks
    // and only erase when the write boundary goes past the sector boundary.
    Erase();
    printf("about to program\n");
    printf("program end %i\n", __flash_binary_end-XIP_BASE);

    multicore_lockout_start_timeout_us(500);
    uint32_t ints = save_and_disable_interrupts();
    flash_range_program(flashOffset+flashPosition*FLASH_SECTOR_SIZE, data, FLASH_SECTOR_SIZE);
    restore_interrupts(ints);
    multicore_lockout_end_timeout_us(500);
    memset(data, 0, FLASH_SECTOR_SIZE);
    flashPosition++;
    writePosition = 0;
}