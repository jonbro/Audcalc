#ifndef GROOVEBOX_H_
#define GROOVEBOX_H_

#include <stdio.h>
#include "pico/stdlib.h"
#include "Instrument.h"
extern "C" {
  #include "ssd1306.h"
}
#include "q15.h"
#include "Midi.h"
class Pattern
{
   public:
      uint8_t notes[16][16][16];
      bool trigger[16][16][16];
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
   void Render(int16_t* buffer,size_t size);
   uint8_t GetInstrumentParamA(int voice);
   uint8_t GetInstrumentParamB(int voice);
   Instrument instruments[4];
   //MacroOscillator osc[2];
   int CurrentStep = 0;
   uint8_t currentVoice = 0;
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

   uint32_t *color;
   Pattern patterns[16];
   Pattern *Editing;
   Pattern *Playing;
   uint8_t notes[16][16][16];
   bool trigger[16][16][16];
   Midi midi;
};
#endif // GROOVEBOX_H_