#pragma once
#include "hardware/flash.h"
#include "pico/multicore.h"
#include <stdio.h>
#include <string.h>

class Serializer
{
    public:
        void    Init();
        void    AddData(uint8_t val);
        void    Finish();
        uint8_t GetNextValue();
    private:
        void    FlushToFlash();
        void    Erase();
        uint32_t writePosition;
        uint32_t readPosition;
        uint32_t flashPosition;
        uint8_t data[FLASH_SECTOR_SIZE];
        bool needsSectorErase;
};