#pragma once

#include "Midi.h"
#include "q15.h"
#include "voice_data.h"
#include "SequencedVoice.h"

// how many notes one midi track can have sounding at once.
// we do voice stealing based on how close to done a note is
#define MIDI_VOICE_NOTE_SLOTS 16

// Playback state for one midi track.
class MidiVoice : public SequencedVoice
{
    public:
        void Init(Midi *_midi);
        void NoteOn(uint8_t key, int16_t midinote, uint8_t step, uint8_t pattern, bool livePlay, VoiceData &voiceData);
        // one instrument clock pulse: ages the gates and fires any due retrigger
        void TempoPulse();
        // release anything still sounding, on the channel it was triggered on
        void AllNotesOff();

    private:
        void Retrigger();
        // allocates (or reuses) a note slot, sends the note on and arms its gate.
        // A note already sounding is re-articulated, not just extended.
        void SendNoteOn(uint8_t channel, int16_t note, uint8_t vel, int16_t gateTicks);
        // base velocity scaled by however far the retrigger fade has walked
        uint8_t RetriggerVelocity();

        Midi *midi = 0;

        struct NoteState
        {
            int16_t ticksRemaining;
            // internal pitch, 12 above the transmitted note. Needs to be wider than
            // int8_t: chord voices and high octaves both reach past 127, and a
            // negative value reads as "free slot" below.
            int16_t note;
            // the channel the note on went out on, so the note off matches even if
            // the track has been repointed since
            uint8_t channel;
        };
        NoteState noteStates[MIDI_VOICE_NOTE_SLOTS];

        // the chord the last trigger put down, kept so retriggers can restrike it
        int16_t chordNotes[MIDI_CHORD_MAX_NOTES];
        uint8_t chordCount = 0;
        uint8_t chordChannel = 0;
        uint8_t chordVelocity = 0;
        int16_t chordGateTicks = 0;
};
