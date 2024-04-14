#include "SongData.h"
#include "m6x118pt7b.h"

SyncMode SongData::GetSyncOutMode(){
    uint8_t bareSyncMode = ((((uint16_t)internalData.syncOut)*6) >> 8);
    SyncMode mode = SyncModeNone;
    switch(bareSyncMode)
    {
        case 1:
            mode = SyncModeMidi;
            break;
        case 2:
            mode = (SyncMode)(SyncModeMidi | SyncModePO);
            break;
        case 3:
            mode = (SyncMode)(SyncModeMidi | SyncMode24);
            break;
        // PO
        case 4:
            mode = SyncModePO;
            break;
        // Volca
        case 5:
            mode = SyncMode24;
            break;
    }
    return mode;
}
SyncMode SongData::GetSyncInMode(){
    uint8_t bareSyncMode = ((((uint16_t)internalData.syncIn)*6) >> 8);
    SyncMode mode = SyncModeNone;
    switch(bareSyncMode)
    {
        case 1:
            mode = SyncModeMidi;
            break;
        case 2:
            mode = (SyncMode)(SyncModeMidi | SyncModePO);
            break;
        case 3:
            mode = (SyncMode)(SyncModeMidi | SyncMode24);
            break;
        // PO
        case 4:
            mode = SyncModePO;
            break;
        // Volca
        case 5:
            mode = SyncMode24;
            break;
    }
    return mode;
}

uint8_t& SongData::GetParam(uint8_t param, uint8_t pattern)
{
    switch(param)
    {
        case 1*2:
            return internalData.changeLength[pattern];
        
        case 19*2:
            return internalData.bpm;
        case 19*2+1:
            return internalData.syncOut;

        case 19*2+(25*2):
            return internalData.bpm;
        case 19*2+1+(25*2):
            return internalData.syncIn;

        case 22*2+(25*2): // this offset to the next page must also be doubled
            return internalData.delayFeedback;
        case 22*2+(25*2)+1: // this offset to the next page must also be doubled
            return internalData.delayTime;
        case 24*2+(25*2): // this offset to the next page must also be doubled
            return internalData.scale;
        case 24*2+1:
            return internalData.root;
    }
    return nothing;
}
uint8_t SongData::GetRoot()
{
    return (internalData.root*12)>>8;
}
uint8_t SongData::GetScale()
{
    return (((uint16_t)internalData.scale)*9)>>8;
}
uint8_t SongData::GetBpm()
{
    return internalData.bpm;
}

uint8_t SongData::GetPlayingPattern()
{
    return internalData.playingPattern;
}
void    SongData::SetPlayingPattern(uint8_t playingPattern)
{
    internalData.playingPattern = playingPattern;
}

void SongData::LoadPatternChain(int *patternChain)
{
    for(int i=0;i<16;i++)
    {
        patternChain[i] = internalData.patternChain[i];
    }
}
void SongData::StorePatternChain(int *patternChain)
{
    for(int i=0;i<16;i++)
    {
        internalData.patternChain[i] = patternChain[i];
    }
}

uint8_t SongData::GetPatternChainLength()
{
    return internalData.patternChainLength;
}
void    SongData::SetPatternChainLength(uint8_t patternChainLength)
{
    internalData.patternChainLength = patternChainLength;
}


const int keyMap[16] = {
    12, 13, 14, 15,
    8, 9, 10, 11,
    4, 5, 6, 7,
    0, 1, 2, 3
};

const int scales[] = {
    0, 2, 4, 5, 7, 9, 11, //Maj
    0, 2, 3, 5, 7, 8, 10, //NMin
    0, 2, 3, 5, 7, 8, 11, //HMin
    0, 2, 3, 5, 7, 9, 11, //MMin
    0, 2, 3, 5, 7, 9, 10, //Dor
    0, 1, 3, 5, 7, 8, 10, //Phry
    0, 2, 4, 6, 7, 9, 11, //Lyd
    0, 2, 4, 5, 7, 9, 10, //Mixo
    0, 1, 3, 5, 6, 8, 10, //Locr
};

uint8_t SongData::GetNote(uint8_t key, int8_t octave)
{
    key = keyMap[key];
    return scales[GetScale()*7+key%7]+12*(key/7)+12*octave+GetRoot() + 60;
}

const char *syncOutStrings[6] = { 
    "none",
    "midi",
    "m+PO",
    "m+24",
    "PO",
    "24pq"
};
const char *rootStrings[12] = { 
    "C",
    "C#",
    "D",
    "D#",
    "E",
    "F",
    "F#",
    "G",
    "G#",
    "A",
    "A#",
    "B"
};


const char *scaleStrings[9] = {
"Maj",
"NMin",
"HMin",
"MMin",
"Dor",
"Phry",
"Lyd",
"Mixo",
"Locr"
};

void SongData::DrawParamString(uint8_t param, uint8_t pattern, char *str, int8_t octave)
{
    ssd1306_t* disp = GetDisplay();
    const uint8_t width = 36;
    const uint8_t column4 = 128-width;
    bool lockA = false, lockB = false;
    char *strA = str;
    char *strB = str+16;
    char *pA = str+32;
    char *pB = str+48;

    switch(param)
    {
        case 1:
            sprintf(strA, "chng");
            sprintf(pA, "%i", internalData.changeLength[pattern]/4+1);
            sprintf(strB, "");
            sprintf(pB,"");
            break;
        case 19:
            sprintf(strA, "bpm");
            sprintf(pA, "%i", internalData.bpm+1);
            sprintf(strB, "SncO");
            sprintf(pB,syncOutStrings[(internalData.syncOut*6)>>8]);
            break;
        case 19+25:
            sprintf(strA, "bpm");
            sprintf(pA, "%i", internalData.bpm+1);
            sprintf(strB, "SncI");
            sprintf(pB,syncOutStrings[(internalData.syncIn*6)>>8]);
            break;
        case 24:
            sprintf(strA, "Octv");
            sprintf(pA, "%i", octave);
            sprintf(strB, "Root");
            sprintf(pB,rootStrings[GetRoot()]);
            break;
        case 24+25:
            sprintf(strA, "Scle");
            sprintf(pA, scaleStrings[GetScale()]);
            sprintf(strB, "");
            sprintf(pB,"");
            break;
        case 22+25:
            sprintf(strA, "DlyFb");
            sprintf(pA, "%i", internalData.delayFeedback);
            sprintf(strB, "DlyT");
            sprintf(pB, "%i", internalData.delayTime);
            break;
    }
    
    ssd1306_draw_string_gfxfont(disp, column4+3, 12, str+32, true, 1, 1, &m6x118pt7b);
    ssd1306_draw_string_gfxfont(disp, column4+3, 17+12, str+48, true, 1, 1, &m6x118pt7b);
    
    ssd1306_draw_string_gfxfont(disp, column4-33, 12, str, true, 1, 1, &m6x118pt7b);    
    ssd1306_draw_string_gfxfont(disp, column4-33, 17+12, str+16, true, 1, 1, &m6x118pt7b);
}

void SongData::Serialize(pb_ostream_t *s)
{
    pb_encode_ex(s, SongDataInternal_fields, &internalData, PB_ENCODE_DELIMITED);
}

void SongData::Deserialize(pb_istream_t *s)
{
    if(!pb_decode_ex(s, SongDataInternal_fields, &internalData, PB_ENCODE_DELIMITED))
    {
        const char * error = PB_GET_ERROR(s);
        printf("SongData deserialize error: %s\n", error);
    }
}
