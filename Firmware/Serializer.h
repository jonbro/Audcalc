#pragma once
#include "hardware/flash.h"
#include "pico/multicore.h"
#include <stdio.h>
#include <string.h>
#include "filesystem.h"

class Serializer
{
    public:
        void    Init();
        void    AddData(uint8_t val);
        void    Finish();
        uint8_t GetNextValue();
        void    Erase();
        ffs_file writeFile;
    private:
        void    FlushToFlash();
        uint32_t writePosition;
        uint32_t readPosition;
        uint32_t flashPosition;
        uint8_t data[256];
        bool needsSectorErase;
};