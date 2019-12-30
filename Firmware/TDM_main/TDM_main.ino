#define USE_SPI_DMA true
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <Encoder.h>
#include <SerialFlash.h>

Encoder Enc1(4, 3);
Encoder Enc2(6, 5);

#include "AudioSamplerSimple.h"

// GUItool: begin automatically generated code
AudioSynthNoiseWhite     noise1;         //xy=104,482
AudioSynthSimpleDrum     drum1;          //xy=246,413
AudioSynthSimpleDrum     drum2;          //xy=251,453
AudioSamplerSimple       playMem1;       //xy=253,525
AudioEffectEnvelope      envelope1;      //xy=261,489
AudioMixer4              mixer1;         //xy=455,501
AudioOutputI2S           i2s1;           //xy=591,498
AudioConnection          patchCord1(noise1, envelope1);
AudioConnection          patchCord2(drum1, 0, mixer1, 0);
AudioConnection          patchCord3(drum2, 0, mixer1, 1);
AudioConnection          patchCord4(playMem1, 0, mixer1, 3);
AudioConnection          patchCord5(envelope1, 0, mixer1, 2);
AudioConnection          patchCord6(mixer1, 0, i2s1, 0);
AudioConnection          patchCord7(mixer1, 0, i2s1, 1);
AudioControlSGTL5000     sgtl5000_1;     //xy=762,495
// GUItool: end automatically generated code
#include "AudioSampleMug.h"
#include "Matrix.h"
Matrix* m;
float start[16];
float length[16];

void setup() {
  Wire.setClock(400000);

  AudioMemory(15);
  sgtl5000_1.enable();
  sgtl5000_1.volume(0.8);
  mixer1.gain(0, 0.36);
  mixer1.gain(1, 0.36);
  mixer1.gain(2, 0.15);
  mixer1.gain(3, 0.15);
  drum2.frequency(300);
  drum2.secondMix(0.4);
  noise1.amplitude(1.0);
  envelope1.attack(2.2);
  envelope1.hold(0.0);
  envelope1.decay(10.4);
  envelope1.sustain(0.0);
  envelope1.release(84.5);
  ScreenSetup();
  m = new Matrix();
  m->setup();
  Wire.begin(); // join i2c bus (address optional for master)
  delay(700);
  Serial.begin(9600);
  //while (!Serial) ;
  Serial.println("test");
  for (int i = 0; i < 16; i++)
  {
    start[i] = 1 / 16.0f * (float)i;
    length[i] = 1 / 16.0f;
  }
}

bool pressed[5][5];
bool lastState[5][5];
bool bang[3][16];
bool onPress[5][5];
static uint32_t next;
int step = 0;
int currentSound = 0;
bool soundSelect;
uint8_t rot = 0;

unsigned long last_time = millis();
unsigned long last_button_update = millis();
unsigned long last_oled_update = millis();
int lorb = 0;

long enc1last  = 0;
long enc2last  = 0;
long enc1d  = 0;
long enc2d  = 0;

void UpdateScreenAndButtons()
{
  if (micros() - last_button_update >= 10)
  {
    if (lorb % 2 == 0)
    {
      m->update();
      for (int x = 0; x < 5; x++)
      {
        for (int y = 0; y < 5; y++)
        {
          bool p = m->readButton(x, y);
          onPress[x][y] = p && !pressed[x][y];
          pressed[x][y] = p;
        }
      }
    }
    else
    {
      ScreenLoop();
    }
    lorb++;
    last_button_update = micros();
  }

  long enc1c = Enc1.read();
  if (enc1c != enc1last) {
    enc1d = enc1c - enc1last;
    enc1last = enc1c;
  }
  else
  {enc1d = 0;}
  long enc2c = Enc2.read();
  if (enc2c != enc2last) {
    enc2d = enc2c - enc2last;
    enc2last = enc2c;
  }
  else
  {enc2d = 0;}
}

int lastPressed = 0;
void loop()
{
  UpdateScreenAndButtons();
  AudioNoInterrupts();
  for (int x = 0; x < 4; x++)
  {
    for (int y = 0; y < 4; y++)
    {
      if(onPress[x][y+1])
      {
        lastPressed = x+y*4;    
        playMem1.setStart(AudioSampleMug, start[lastPressed]);
        playMem1.setLength(AudioSampleMug, length[lastPressed]);
        playMem1.play(AudioSampleMug);
      }
    }
  }
  start[lastPressed] = max(0.0f, min(1.0f, 0.001f*((float)enc1d)+start[lastPressed]));
  length[lastPressed] = max(0.0f, min(1.0f, 0.001f*((float)enc2d)+length[lastPressed]));
  AudioInterrupts();
  if (millis() >= next)
  {
    m->setLed((step - 1 + 16) % 4, ((step - 1 + 16) / 4) % 4 + 1, bang[currentSound][(step - 1 + 16) % 16] ? 16 : 0);
    m->setLed(step % 4, (step / 4) % 4 + 1, 4);

    next = millis() + 125;
    step++;
  }

}
void oldLoop() {
  if (micros() - last_button_update >= 10)
  {
    if (lorb % 2 == 0)
    {
      m->update();
      for (int x = 0; x < 5; x++)
      {
        for (int y = 0; y < 5; y++)
        {
          pressed[x][y] = m->readButton(x, y);
        }
      }
    }
    else
    {
      ScreenLoop();
    }
    lorb++;
    last_button_update = micros();
  }

  soundSelect = pressed[0][0];
  if (soundSelect)
  {
    if (pressed[0][1])
    {
      currentSound = 0;
    }
    else if (pressed[1][1])
    {
      currentSound = 1;
    }
    else if (pressed[2][1])
    {
      currentSound = 2;
    }
    else if (pressed[3][1])
    {
      playMem1.play(AudioSampleMug);
    }
  }
  else
  {
    for (int x = 0; x < 4; x++)
    {
      for (int y = 1; y < 5; y++)
      {
        if (pressed[x][y] && !lastState[x][y])
        {
          bang[currentSound][x + (y - 1) * 4] = !bang[currentSound][x + (y - 1) * 4];
          m->setLed(x, y, bang[currentSound][x + (y - 1) * 4] ? 16 : 0);
        }
      }
    }
  }
  for (int x = 0; x < 5; x++)
  {
    for (int y = 0; y < 5; y++)
    {
      lastState[x][y] = pressed[x][y];
    }
  }

  if (millis() >= next)
  {
    m->setLed((step - 1 + 16) % 4, ((step - 1 + 16) / 4) % 4 + 1, bang[currentSound][(step - 1 + 16) % 16] ? 16 : 0);
    m->setLed(step % 4, (step / 4) % 4 + 1, 4);

    next = millis() + 125;
    AudioNoInterrupts();
    if (bang[0][step % 16])
    {
      drum1.noteOn();
    }
    if (bang[1][step % 16])
    {
      drum2.noteOn();
    }
    if (bang[2][step % 16])
    {
      envelope1.noteOn();
    }
    AudioInterrupts();
    step++;
  }
}
