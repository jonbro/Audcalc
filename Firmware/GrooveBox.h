#ifndef GROOVEBOX_H_
#define GROOVEBOX_H_

#include <stdio.h>
#include "pico/stdlib.h"
extern "C" {
#include "ssd1306.h"
}
class Pattern
{
   public:
      uint8_t notes[16][16];
      bool trigger[16][16];
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
   uint8_t GetInstrumentParamA(int voice);
   uint8_t GetInstrumentParamB(int voice);
   int CurrentStep = 0;
   uint8_t currentVoice = 0;
 private:
   uint8_t instrumentParamA[5];
   uint8_t instrumentParamB[5];
   int needsNoteTrigger = -1;
   int drawY = 0;
   int lastNotePlayed = 0;
   bool playing = false;
   bool writing = false;
   bool holdingWrite = false;
   bool liveWrite = false;
   bool soundSelectMode = false;
   uint32_t *color;
   Pattern patterns[16];
   Pattern *Editing;
   Pattern *Playing;
   uint8_t notes[16][16];
   bool trigger[16][16];
};
#endif // GROOVEBOX_H_