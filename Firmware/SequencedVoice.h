#pragma once

#include "q15.h"
#include "voice_data.h"

// Parent class of the midi voice and instrument voice. The only shared code between them is
// retriggering. These are not polymorphic, to save the vtable overhead. Not currently necessary
// because midi and non-midi are stored in two seperate arrays
class SequencedVoice
{
    public:
        // Retrigger period in instrument clock pulses. The instrument clock runs at
        // 96ppq
        static uint8_t RetriggerPulses(uint8_t rawSpeed);

    protected:
        // Arms the retrigger countdown from an already resolved param cache. Called
        // for every note, live played or sequenced
        void ArmRetriggers(const uint8_t *pv);
        // Advances the countdown by one pulse.
        bool TickRetriggers();

        // where this note came from, for looking up parameter locks
        VoiceData *playingVoice = 0;
        uint8_t playingStep = 0;
        uint8_t playingPattern = 0;
        uint8_t lastPressedKey = 0;
        // a live played note has no step of its own, so it resolves no locks
        bool liveTriggered = false;

        uint8_t retriggerNextPulse = 0;
        uint8_t retriggersRemaining = 0;
        // multiplier walked by retriggerFade
        q15_t retriggerVolume = 0x7fff;
        q15_t retriggerFade = 0;
};
