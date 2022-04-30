#ifndef GROOVEBOX_H_
#define GROOVEBOX_H_

#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/time.h"

#include "Instrument.h"
extern "C" {
  #include "ssd1306.h"
}
#include "q15.h"
#include "Midi.h"
#include "Serializer.h"

class Pattern
{
   public:
      uint8_t notes[16*16*16];
      bool trigger[16*16*16];
};
class GrooveBox {
 public:
   GrooveBox(uint32_t *_color);
   void OnKeyUpdate(uint key, bool pressed);
   void UpdateAfterTrigger(uint step);
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
   Instrument instruments[4];
   int CurrentStep = 0;
   uint8_t currentVoice = 0;
  bool recording = false;
  uint32_t recordingLength;
  bool erasing = false;

 private:
   uint8_t instrumentParamA[8];
   uint8_t instrumentParamB[8];
   int needsNoteTrigger = -1;
   int drawY = 0;
   int lastNotePlayed = 0;
   
   int patternChain[16] = {0};
   int patternChainLength = 1;
   int chainStep = 0;
   bool nextPatternSelected = false;

   bool playing = false;
   bool writing = false;
   bool holdingWrite = false;
   bool liveWrite = false;
   bool soundSelectMode = false;
   bool patternSelectMode = false;
   bool paramSelectMode = false;
   uint8_t param = 0;
   uint32_t *color;
   Pattern patterns[16];
   Pattern *Editing;
   Pattern *Playing;
   uint8_t notes[16*16*16];
   uint8_t trigger[16*16*16]; // could probably use the notes array above for this, just have a "off" value
   Midi midi;
   uint32_t bpm = 105;
   uint16_t nextTrigger = 0;
   uint8_t lastAdcValA = 0;
   uint8_t lastAdcValB = 0;

   ffs_file files[16];
};
#endif // GROOVEBOX_H_