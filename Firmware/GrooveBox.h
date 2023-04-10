#ifndef GROOVEBOX_H_
#define GROOVEBOX_H_

#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "pico/rand.h"

#include "Instrument.h"
extern "C" {
  #include "ssd1306.h"
}
#include "q15.h"
#include "Midi.h"
#include "Serializer.h"
#include "hardware.h"
#include "voice_data.h"
#include "GlobalData.h"
#include "usb_microphone.h"
#include "audio/resources.h"
#include "Reverb2.h"
#include "Delay.h"
#include "MidiParamMapper.h"

#define VOICE_COUNT 8

class GrooveBox {
 public:
  GrooveBox(uint32_t *_color);
  void OnKeyUpdate(uint key, bool pressed);
  bool GetTrigger(uint voice, uint step, uint8_t &note, uint4 &key);
  void UpdateDisplay(ssd1306_t *p);
  void OnAdcUpdate(uint16_t a, uint16_t b);
  void SetGlobalParameter(uint8_t a, uint8_t b, bool setA, bool setB);
  bool IsPlaying();
  int GetNote();
  int GetNoteMidi();
  void Render(int16_t* output_buffer, int16_t* input_buffer, size_t size);
  uint8_t GetInstrumentParamA(int voice);
  uint8_t GetInstrumentParamB(int voice);
  void Serialize();
  void Deserialize(); 
  void OnFinishRecording();
  int GetLostLockCount();
  Instrument instruments[VOICE_COUNT];
  int CurrentStep = 0;
  uint8_t currentVoice = 0;
  bool recording = false;
  uint32_t recordingLength;
  bool erasing = false;
  bool playThroughEnabled = false;
  uint8_t GetCurrentPattern()
  {
    return playingPattern;//patternChain[chainStep];
  }

  void ResetADCLatch()
  {
    paramSetA = paramSetB = false;
  }

  void ResetPatternOffset()
  {
    for (size_t i = 0; i < 17; i++)
    {
        patternStep[i] = 0;
        beatCounter[i] = 0;
        if(i<16)
          patternLoopCount[i] = 0;
    }
    beatCounter[17] = 0;
  }
  void OnCCChanged(uint8_t cc, uint8_t newValue);

 private:
  MidiParamMapper midiMap;
  bool needsInitialADC; 
  void TriggerInstrument(uint4 key, int16_t midi_note, uint8_t step, uint8_t pattern, bool livePlay, VoiceData &voiceData, int channel);
  void TriggerInstrumentMidi(int16_t midi_note, uint8_t step, uint8_t pattern, VoiceData &voiceData, int channel);
  void CalculateTempoIncrement();
  uint8_t voiceCounter = 0;
  uint8_t instrumentParamA[8];
  uint8_t instrumentParamB[8];
  int needsNoteTrigger = -1;
  int drawY = 0;
  uint16_t drawCount = 0;
  int lastNotePlayed = 60;
  uint4 lastKeyPlayed = {0};
  bool paramSetA, paramSetB;
  uint32_t tempoPhaseIncrement = 0, tempoPhase = 0;
  uint8_t beatCounter[18] = {0};
  bool playing = false;
  bool writing = false;
  bool holdingWrite = false;
  bool holdingEscape = false;
  bool holdingMute = false;
  bool holdingArm = false;
  uint8_t recordingTarget = 0;
  int clearTime = -1;
  int shutdownTime = -1;
  bool liveWrite = false;
  bool soundSelectMode = false;
  bool patternSelectMode = false;
  bool paramSelectMode = false;
  
  // used for midi learn mode
  int16_t lastEditedParam = -1;
  
  bool selectedGlobalParam = false;
  uint8_t param = 0;
  uint32_t *color;
  uint16_t hadTrigger = 0;
  // defaults to allow all playback
  uint16_t allowPlayback = 0xffff;
  // each sound can be on a different step through the pattern, we should track these
  uint8_t patternStep[17] = {0};

  // for tracking how many times we've looped through a pattern
  uint8_t patternLoopCount[16] = {0};

  uint32_t framesSinceLastTouch = 0;
  // the page we are currently editing for each sound
  // clamped to the length of this pattern / sound
  uint8_t editPage[16] = {0};
  uint8_t playingPattern = 0;
  int patternChain[16] = {0};
  int8_t voiceChannel[16] = {-1};
  
  int patternChainLength = 1;
  int chainStep = 0;
  bool nextPatternSelected = false;
  uint8_t storingParamLockForStep = 0;
  bool parameterLocked;
  VoiceData patterns[16];
  VoiceData *Editing;
  VoiceData *Playing;
  GlobalData globalData;
  Midi midi;
  uint16_t nextTrigger = 0;
  uint8_t lastAdcValA = 0;
  uint8_t lastAdcValB = 0;
  uint16_t AdcInterpolatedA = 0;
  uint16_t AdcInterpolatedB = 0;
  Delay2 delay;
  Reverb2 verb;
  ffs_file files[16];
  int64_t renderTime = 0;
  int64_t sampleCount = 0;
};

extern GrooveBox *groovebox; // used for the midi callbacks

#endif // GROOVEBOX_H_