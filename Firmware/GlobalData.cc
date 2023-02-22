#include "GlobalData.h"
#include "m6x118pt7b.h"

SyncOutMode GlobalData::GetSyncOutMode(){
    uint8_t bareSyncMode = ((((uint16_t)syncOut)*6) >> 8);
    SyncOutMode mode = SyncOutModeNone;
    switch(bareSyncMode)
    {
        case 1:
            mode = SyncOutModeMidi;
            break;
        case 2:
            mode = (SyncOutMode)(SyncOutModeMidi | SyncOutModePO);
            break;
        case 3:
            mode = (SyncOutMode)(SyncOutModeMidi | SyncOutMode24);
            break;
        // PO
        case 4:
            mode = SyncOutModePO;
            break;
        // Volca
        case 5:
            mode = SyncOutMode24;
            break;
    }
    return mode;
}

uint8_t& GlobalData::GetParam(uint8_t param, uint8_t pattern)
{
    switch(param)
    {
        case 1*2:
            return changeLength[pattern];
        case 19*2:
            return bpm;
        case 19*2+1:
            return syncOut;
        case 24*2:
            return octave;
        case 24*2+(25*2): // this offset to the next page must also be doubled
            return scale;
        case 24*2+1:
            return root;
    }
    return nothing;
}
uint8_t GlobalData::GetRoot()
{
    return (root*12)>>8;
}
uint8_t GlobalData::GetScale()
{
    return (((uint16_t)scale)*9)>>8;
}

int8_t GlobalData::GetOctave()
{
    return ((int8_t)(octave/51))-2;
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

uint8_t GlobalData::GetNote(uint8_t key)
{
    key = keyMap[key];
    return scales[GetScale()*7+key%7]+12*(key/7)+12*GetOctave()+GetRoot() + 60;
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

void GlobalData::DrawParamString(uint8_t param, uint8_t pattern, char *str)
{
    ssd1306_t* disp = GetDisplay();
    uint8_t width = 36;
    uint8_t column4 = 128-width;
    bool lockA = false, lockB = false;
    char *strA = str;
    char *strB = str+16;
    char *pA = str+32;
    char *pB = str+48;

    switch(param)
    {
        case 1:
            sprintf(strA, "chng");
            sprintf(pA, "%i", changeLength[pattern]/4+1);
            sprintf(strB, "");
            sprintf(pB,"");
            break;
        case 19:
            sprintf(strA, "bpm");
            sprintf(pA, "%i", bpm+1);
            sprintf(strB, "Sync");
            sprintf(pB,syncOutStrings[(syncOut*6)>>8]);
            break;
        case 24:
            sprintf(strA, "Octv");
            sprintf(pA, "%i", GetOctave());
            sprintf(strB, "Root");
            sprintf(pB,rootStrings[GetRoot()]);
            break;
        case 24+25:
            sprintf(strA, "Scle");
            sprintf(pA, scaleStrings[GetScale()]);
            sprintf(strB, "");
            sprintf(pB,"");
            break;
    }
    
    ssd1306_draw_string_gfxfont(disp, column4+3, 12, str+32, true, 1, 1, &m6x118pt7b);
    ssd1306_draw_string_gfxfont(disp, column4+3, 17+12, str+48, true, 1, 1, &m6x118pt7b);
    
    ssd1306_draw_string_gfxfont(disp, column4-33, 12, str, true, 1, 1, &m6x118pt7b);    
    ssd1306_draw_string_gfxfont(disp, column4-33, 17+12, str+16, true, 1, 1, &m6x118pt7b);
}
