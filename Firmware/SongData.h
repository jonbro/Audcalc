#ifndef GLOBAL_DATA_H_
#define GLOBAL_DATA_H_
#include <stdio.h>
#include <string.h>
extern "C" {
  #include "ssd1306.h"
}

#include "SongDataInternal.pb.h"
#include <pb_encode.h>
#include <pb_decode.h>

enum SyncOutMode { 
    SyncOutModeNone = 0,
    SyncOutModeMidi = 1 << 0,
    SyncOutModePO   = 1 << 1,
    SyncOutMode24   = 1 << 2,
};

class SongData
{
    public:
        SongData()
        {
            InitDefaults();
        }
        void InitDefaults()
        {
            for (size_t i = 0; i < 16; i++)
            {
                internalData.changeLength[i] = 15*4; // need to up this to fit into 0xff
            }
            internalData.syncOut = 0x2b; // this is equal to mode 1
            internalData.bpm = 119;
            internalData.octave = 0x7f;
        }
        uint8_t GetLength(uint8_t pattern)
        {
            return internalData.changeLength[pattern]/4+1;
        }
        uint8_t GetNote(uint8_t key);
        int8_t GetOctave();
        uint8_t GetRoot();
        uint8_t GetBpm();
        uint8_t GetScale();
        SyncOutMode GetSyncOutMode();
        void DrawParamString(uint8_t param, uint8_t pattern, char *str);
        uint8_t& GetParam(uint8_t param, uint8_t pattern);

        void Serialize(pb_ostream_t *s);
        void Deserialize(pb_istream_t *s);

    private:
        SongDataInternal internalData;
        uint8_t nothing; // used for returning a reference when we don't want it to do anything
};

#endif // GLOBAL_DATA_H_