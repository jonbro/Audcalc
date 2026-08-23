#include "SequencedVoice.h"
#include "GrooveBox.h"

uint8_t SequencedVoice::RetriggerPulses(uint8_t rawSpeed)
{
    uint8_t pulses = (((uint16_t)rawSpeed*8)>>8) * 4;
    uint8_t midiCeiling = GrooveBox::getTickCountForRateIndex(7);
    return pulses < midiCeiling ? midiCeiling : pulses;
}

void SequencedVoice::ArmRetriggers(const uint8_t *pv)
{
    retriggersRemaining = ((uint16_t)pv[RetriggerLength]*8)>>8;
    retriggerNextPulse = RetriggerPulses(pv[RetriggerSpeed]);
    
    // don't apply fade if there are no retriggers
    // this could lead to the note not sounding if its a fade in with zero retriggers
    if(retriggersRemaining == 0)
    {
        retriggerVolume = f32_to_q15(1.0f);
        retriggerFade = 0;
        return;
    }

    int16_t fade = (int16_t)pv[RetriggerFade] << 7;
    fade = (fade-0x3fff)*-2; // center at zero
    // fade out the retriggers - the multiplier starts at full and walks down
    if(fade > 0)
        retriggerVolume = sub_q15(f32_to_q15(1.0f), fade);
    else
        retriggerVolume = f32_to_q15(1.0f);
    // how much we change on every strike
    retriggerFade = fade / retriggersRemaining;
}

bool __not_in_flash_func(SequencedVoice::TickRetriggers)()
{
    // playingVoice is only null before the first note, when nothing is armed anyway
    if(retriggersRemaining == 0 || playingVoice == 0)
        return false;
    // RetriggerPulses floors the period, so this is never zero here. The check keeps
    // the decrement from underflowing to 255 if that floor ever moves.
    if(retriggerNextPulse > 0)
        retriggerNextPulse -= 1;
    if(retriggerNextPulse != 0)
        return false;
    retriggersRemaining--;
    retriggerNextPulse = RetriggerPulses(
        playingVoice->GetParamValue(RetriggerSpeed, lastPressedKey, playingStep, playingPattern, !liveTriggered));
    return true;
}
