#include "MidiParamMapper.h"

MidiParamMapper::MidiParamMapper()
{
    for(int i=0;i<128;i++)
    {
        paramMap[i].voice = 0xff;
    }
}

void MidiParamMapper::SetCCTarget(uint8_t cc, uint8_t voice, uint8_t param, uint8_t keyTarget)
{
    assert(cc < 128);
    assert(voice < 16);
    paramMap[cc].voice = voice;
    paramMap[cc].param = param;
    paramMap[cc].keyTarget = keyTarget;
}

void MidiParamMapper::UpdateCC(VoiceData voiceData[], uint8_t cc, uint8_t newValue, uint8_t currentPattern)
{
    assert(cc < 128);
    if(paramMap[cc].voice < 16)
    {
        uint8_t& current_a = voiceData[paramMap[cc].voice].GetParam(paramMap[cc].param, paramMap[cc].keyTarget, currentPattern);
        current_a = newValue;
    }
}
