#pragma once

#include "hardware/uart.h"
#include "pico/stdlib.h"
#include "voice_data.h"

struct MidiParamMap
{
    uint8_t voice;
    uint8_t param;
    uint8_t keyTarget; // for parameters that have per key values (i.e. sample loop points)
};

class MidiParamMapper
{
public:
    MidiParamMapper();
    void UpdateCC(VoiceData voiceData[], uint8_t cc, uint8_t newValue, uint8_t currentPattern);
    void SetCCTarget(uint8_t cc, uint8_t voice, uint8_t param, uint8_t keyTarget);
private:
    MidiParamMap paramMap[128];
};