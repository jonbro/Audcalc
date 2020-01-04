#define USE_SPI_DMA true
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <Encoder.h>
#include <SerialFlash.h>

#include "Sampler.h"

#include "AudioSamplerSimple.h"
Sampler *sampler;
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
  sampler = new Sampler(&playMem1);
  Wire.begin(); // join i2c bus (address optional for master)
  delay(700);
  Serial.begin(9600);
  //while (!Serial) ;
  Serial.println("test");
}


void loop()
{
  sampler->update();
}
