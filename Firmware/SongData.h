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

enum SyncMode { 
    SyncModeNone = 0,
    SyncModeMidi = 1 << 0,
    SyncModePO   = 1 << 1,
    SyncMode24   = 1 << 2,
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
                internalData.patternChain[i] = 0;
            }
            internalData.patternChainLength = 1;
            internalData.playingPattern = 0;
            internalData.syncOut = 0x2b; // this is equal to mode 1
            internalData.syncIn = 0x0; // this is equal to mode 1
            internalData.bpm = 119;
            internalData.delayFeedback  = 0x7f;
            internalData.delayTime      = 0x7f;
        }
        uint8_t GetLength(uint8_t pattern)
        {
            return internalData.changeLength[pattern]/4+1;
        }
        uint8_t GetDelayFeedback(){
            return internalData.delayFeedback;
        }
        uint8_t GetDelayTime(){
            return internalData.delayTime;
        }

        uint8_t GetNote(uint8_t key, int8_t octave);
        uint8_t GetRoot();
        uint8_t GetBpm();
        uint8_t GetScale();
        uint8_t GetPattern();
        
        uint8_t GetPlayingPattern();
        void    SetPlayingPattern(uint8_t playingPattern);

        void LoadPatternChain(int *patternChain);
        void StorePatternChain(int *patternChain);

        uint8_t GetPatternChainLength();
        void    SetPatternChainLength(uint8_t patternChainLength);

        SyncMode GetSyncOutMode();
        SyncMode GetSyncInMode();
        void DrawParamString(uint8_t param, uint8_t pattern, char *str, int8_t octave);
        uint8_t& GetParam(uint8_t param, uint8_t pattern);

        void Serialize(pb_ostream_t *s);
        void Deserialize(pb_istream_t *s);

    private:
        SongDataInternal internalData;
        uint8_t nothing; // used for returning a reference when we don't want it to do anything
};

#endif // GLOBAL_DATA_H_