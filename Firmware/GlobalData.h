#ifndef GLOBAL_DATA_H_
#define GLOBAL_DATA_H_
#include <stdio.h>
#include <string.h>
extern "C" {
  #include "ssd1306.h"
}
enum SyncOutMode { 
    SyncOutModeNone = 0,
    SyncOutModeMidi = 1 << 0,
    SyncOutModePO   = 1 << 1,
    SyncOutMode24   = 1 << 2,
};

class GlobalData
{
    public:
        GlobalData()
        {
            for (size_t i = 0; i < 16; i++)
            {
                changeLength[i] = 15*4; // need to up this to fit into 0xff
            }
        }
        uint8_t GetLength(uint8_t pattern)
        {
            return changeLength[pattern]/4+1;
        }

        SyncOutMode GetSyncOutMode();
        void DrawParamString(uint8_t param, uint8_t pattern, char *str);
        uint8_t& GetParam(uint8_t param, uint8_t pattern);
        uint8_t changeLength[16];
        uint8_t syncIn;
        uint8_t syncOut = 0;
        uint8_t bpm;
        uint8_t scale;
        uint8_t chromatic;
        uint8_t nothing; // used for returning a reference when we don't want it to do anything
};

#endif // GLOBAL_DATA_H_