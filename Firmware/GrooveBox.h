#ifndef GROOVEBOX_H_
#define GROOVEBOX_H_

#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/time.h"
#include <pico/bootrom.h>

#include "Instrument.h"
extern "C" {
  #include "ssd1306.h"
}
#include "q15.h"
#include "Midi.h"
#include "Serializer.h"
#include "reverb.h"
#include "hardware.h"
#include "voice_data.h"
#include "usb_microphone.h"
#include "audio/resources.h"

#define VOICE_COUNT 8


class GrooveBox {
 public:
  GrooveBox(uint32_t *_color);
  void OnKeyUpdate(uint key, bool pressed);
  int GetTrigger(uint voice, uint step);
  void UpdateDisplay(ssd1306_t *p);
  void OnAdcUpdate(uint8_t a, uint8_t b);
  bool IsPlaying();
  int GetNote();
  void Render(int16_t* output_buffer, int16_t* input_buffer, size_t size);
  uint8_t GetInstrumentParamA(int voice);
  uint8_t GetInstrumentParamB(int voice);
  void Serialize();
  void Deserialize(); 
  Instrument instruments[VOICE_COUNT];
  int CurrentStep = 0;
  uint8_t currentVoice = 0;
  bool recording = false;
  uint32_t recordingLength;
  bool erasing = false;
  bool playThroughEnabled = false;

  uint8_t GetCurrentPattern()
  {
    return patternChain[chainStep];
  }
  void ResetADCLatch()
  {
    paramSetA = paramSetB = false;
  }
  void ResetPatternOffset()
  {
    for (size_t i = 0; i < 16; i++)
    {
       patternStep[i] = beatCounter[i] = 0;
    }
    
  }
 private:
  bool needsInitialADC; 
  void TriggerInstrument(int16_t pitch, int16_t midi_note, uint8_t step, uint8_t pattern, bool livePlay, VoiceData &voiceData, int channel);
  void CalculateTempoIncrement();
  uint8_t voiceCounter = 0;
  uint8_t instrumentParamA[8];
  uint8_t instrumentParamB[8];
  int needsNoteTrigger = -1;
  int drawY = 0;
  int lastNotePlayed = 0;
  bool paramSetA, paramSetB;
  uint32_t tempoPhaseIncrement = 0, tempoPhase = 0;
  uint8_t beatCounter[16] = {0};
  bool playing = false;
  bool writing = false;
  bool holdingWrite = false;
  bool holdingEscape = false;
  bool holdingArm = false;
  uint8_t recordingTarget = 0;
  int clearTime = -1;
  int shutdownTime = -1;
  bool liveWrite = false;
  bool soundSelectMode = false;
  bool patternSelectMode = false;
  bool paramSelectMode = false;
  
  uint8_t param = 0;
  uint32_t *color;
  // each sound can be on a different step through the pattern, we should track these
  uint8_t patternStep[16] = {0};

  // the page we are currently editing for each sound
  // clamped to the length of this pattern / sound
  uint8_t editPage[16] = {0};
  
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

  Midi midi;
  uint16_t nextTrigger = 0;
  uint8_t lastAdcValA = 0;
  uint8_t lastAdcValB = 0;
  Delay delay;
  ffs_file files[16];
  VoiceData *globalParams;
};
#endif // GROOVEBOX_H_