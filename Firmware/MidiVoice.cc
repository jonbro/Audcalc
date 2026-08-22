#include "MidiVoice.h"
#include "GrooveBox.h"

void MidiVoice::Init(Midi *_midi)
{
    midi = _midi;
    for(int i=0;i<MIDI_VOICE_NOTE_SLOTS;i++)
    {
        noteStates[i].note = -1;
        noteStates[i].ticksRemaining = -1;
        noteStates[i].channel = 0;
    }
}

void MidiVoice::AllNotesOff()
{
    for(int i=0;i<MIDI_VOICE_NOTE_SLOTS;i++)
    {
        if(noteStates[i].note < 0)
            continue;
        midi->NoteOff(noteStates[i].channel, noteStates[i].note-12);
        noteStates[i].note = -1;
        noteStates[i].ticksRemaining = -1;
    }
    chordCount = 0;
}

uint8_t MidiVoice::RetriggerVelocity()
{
    // retriggerVolume walks from its start value towards (or away from) unity as
    // the retriggers fire; on midi that fade lands on velocity
    int32_t scaled = ((int32_t)chordVelocity * retriggerVolume) >> 15;
    if(scaled < 1) scaled = 1;   // 0 would read as a note off
    if(scaled > 127) scaled = 127;
    return (uint8_t)scaled;
}

void MidiVoice::SendNoteOn(uint8_t channel, int16_t note, uint8_t vel, int16_t gateTicks)
{
    // everything reaches the wire through here. The transmitted pitch is 12 below
    // the internal one, so anything outside this range would send a byte with the
    // high bit set - the receiver reads that as a status byte and the stream desyncs
    if(note < 12 || note > 139)
        return;
    for(int i=0;i<MIDI_VOICE_NOTE_SLOTS;i++)
    {
        // note is -1 on a free slot, so don't let a negative pitch match one
        if(noteStates[i].note < 0 || noteStates[i].note != note)
            continue;
        // still holding from an earlier trigger. Re-play it rather instead of extending the gate.
        // this is somewhat of a judgment call.
        midi->NoteOff(noteStates[i].channel, noteStates[i].note-12);
        midi->NoteOn(channel, note-12, vel);
        noteStates[i].channel = channel;
        noteStates[i].ticksRemaining = gateTicks;
        return;
    }
    // otherwise take a free slot, falling back to stealing the note closest to done
    int16_t lowestTime = 0x7fff;
    uint8_t lowestTimeIdx = 0;
    for(int i=0;i<MIDI_VOICE_NOTE_SLOTS;i++)
    {
        if(noteStates[i].note < 0)
        {
            midi->NoteOn(channel, note-12, vel);
            noteStates[i].note = note;
            noteStates[i].channel = channel;
            noteStates[i].ticksRemaining = gateTicks;
            return;
        }
        if(noteStates[i].ticksRemaining < lowestTime)
        {
            lowestTime = noteStates[i].ticksRemaining;
            lowestTimeIdx = i;
        }
    }
    // only steal the slot if the note its holding gets correctly released (the midi send succeeds)
    if(!midi->NoteOff(noteStates[lowestTimeIdx].channel, noteStates[lowestTimeIdx].note-12))
        return;
    midi->NoteOn(channel, note-12, vel);
    noteStates[lowestTimeIdx].note = note;
    noteStates[lowestTimeIdx].channel = channel;
    noteStates[lowestTimeIdx].ticksRemaining = gateTicks;
}

void __not_in_flash_func(MidiVoice::Retrigger)()
{
    retriggerVolume = add_q15(retriggerVolume, retriggerFade);
    // restrike the whole chord, not just the root
    uint8_t vel = RetriggerVelocity();
    for(int i=0;i<chordCount;i++)
    {
        SendNoteOn(chordChannel, chordNotes[i], vel, chordGateTicks);
    }
}

void __not_in_flash_func(MidiVoice::TempoPulse)()
{
    for(int i=0;i<MIDI_VOICE_NOTE_SLOTS;i++)
    {
        if(noteStates[i].note < 0)
            continue;
        // stop at zero rather than running the counter negative - an expired note
        // can sit here for several pulses while the tx buffer drains
        if(noteStates[i].ticksRemaining > 0)
            noteStates[i].ticksRemaining--;
        if(noteStates[i].ticksRemaining <= 0)
        {
            // if the write was dropped (a chord retrigger can outrun the 31250 baud
            // din link) keep the note and retry next pulse, otherwise it hangs
            if(midi->NoteOff(noteStates[i].channel, noteStates[i].note-12))
                noteStates[i].note = -1;
        }
    }
    if(TickRetriggers())
        Retrigger();
}

void __not_in_flash_func(MidiVoice::NoteOn)(uint8_t key, int16_t midinote, uint8_t step, uint8_t pattern, bool livePlay, VoiceData &voiceData)
{
    if(livePlay)
    {
        lastPressedKey = key;
    }
    playingStep = step;
    playingPattern = pattern;
    playingVoice = &voiceData;

    uint8_t pv[VoiceData::PARAM_CACHE_SIZE];
    voiceData.FillResolvedParamCache(playingStep, playingPattern, lastPressedKey, pv);

    ArmRetriggers(pv);

    uint8_t midiChannel = voiceData.GetMidiChannel();
    uint8_t holdIndex = pv[MidiHold]>>4;
    uint16_t stepPulses = GrooveBox::getTickCountForRateIndex((voiceData.GetRateForPattern(playingPattern)*7)>>8);
    // the table is in twenty-fourths of a step. Keep the multiply in 32 bits: the
    // slowest rate at the longest gate is 192*576, which doesn't fit in 16.
    int16_t gatePulses = (int16_t)(((uint32_t)stepPulses * midiHoldTwentyFourths[holdIndex])/24);

    // get this step's cc values out ahead of the notes; only changed ones are sent
    // we send this in advance to changes apply to this note immediately (including program changes)
    voiceData.SendMidiCCs(midi, pv);

    // notes still held on another channel belong to the channel this voice used
    // before, so release them before it takes the new one over
    if(chordChannel != midiChannel)
        AllNotesOff();

    chordChannel = midiChannel;
    chordVelocity = pv[Timbre]>>1;
    chordGateTicks = gatePulses;
    chordCount = VoiceData::BuildChord(midinote, pv[ChordVoices], pv[ChordShape], chordNotes);

    uint8_t vel = RetriggerVelocity();
    for(int i=0;i<chordCount;i++)
    {
        SendNoteOn(chordChannel, chordNotes[i], vel, chordGateTicks);
    }
}
