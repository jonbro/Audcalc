#include "Sampler.h"

/*******
   ARRRGH STUPID HACK, JUST COPY PASTED THE WHOLE SCREEN FILE HERE
*/
#include "Screen.h"

Sampler::Sampler(AudioSamplerSimple* samplerCore)
  : Enc1(4, 3), Enc2(6, 5),
    step(0), next(0), writeEnabled(false),
    state(Normal), currentChannel(0), lastPressed(0)
{

  this->samplerCore = samplerCore;
  this->m.setup();
  this->ioCount = 0;
  this->lastIOUpdate = -1;

  for (int i = 0; i < 16; i++)
  {
    pattern[i] = 0;
    start[i] = 1 / 16.0f * (float)i;
    length[i] = 1 / 16.0f;
  }

  screen_setup();
}

void Sampler::update()
{
  this->updateIO();
  if (millis() >= next)
  {
    nextStep();
  }
}
void Sampler::nextStep()
{
  next = millis() + 125;
  step = (step + 1) % 16;
  if (pattern[step])
  {
    samplerCore->setStart(AudioSampleMug, start[pattern[step] - 1]);
    samplerCore->setLength(AudioSampleMug, length[pattern[step] - 1]);
    samplerCore->play(AudioSampleMug);
  }
}

// handle the input output stuff
void Sampler::updateIO()
{
  int currentMicros = micros();
  if (currentMicros - this->lastIOUpdate >= 10)
  {
    lastIOUpdate = currentMicros;
    if (this->ioCount % 2 == 0)
    {
      m.update();
      for (int x = 0; x < 5; x++)
      {
        for (int y = 0; y < 5; y++)
        {
          bool p = m.readButton(x, y);
          onPress[x][y] = p && !pressed[x][y];
          pressed[x][y] = p;
        }
      }
      // sound select
      if (pressed[0][0])
      {
        state = Sound;
        // handle channel selection
        // todo, would be nice to have shorter way to do this loop
        // probably doing a 1d array would be better
        for (int x = 0; x < 4; x++)
        {
          for (int y = 1; y < 5; y++)
          {
            if (onPress[x][y])
            {
              currentChannel = x + (y - 1) * 4;
            }
            m.setLed(x, y, currentChannel == x + (y - 1) * 4);
          }
        }
      }
      else if (pressed[1][0])
      {
        state = Pattern;
      }
      else
      {
        state = Normal;
        for (int x = 0; x < 4; x++)
        {
          for (int y = 1; y < 5; y++)
          {
            m.setLed(x, y, step % 16 == x + (y - 1) * 4 || pattern[x + (y - 1) * 4]);
            if (onPress[x][y])
            {
              if (writeEnabled)
              {
                // clear the step if necessary
                if (pattern[x + (y - 1) * 4] == lastPressed + 1)
                {
                  pattern[x + (y - 1) * 4] = 0;
                }
                else
                {
                  pattern[x + (y - 1) * 4] = lastPressed + 1;
                }
              }
              else
              {
                // trigger and store the most recently pressed button
                samplerCore->setStart(AudioSampleMug, start[x + (y - 1) * 4]);
                samplerCore->setLength(AudioSampleMug, length[x + (y - 1) * 4]);
                samplerCore->play(AudioSampleMug);
                lastPressed = x + (y - 1) * 4;
              }
            }
          }
        }
      }
      // toggle write state
      if (onPress[4][4])
      {
        writeEnabled = !writeEnabled;
        m.setLed(4, 4, writeEnabled ? 1 : 0);
      }
    }
    else
    {
      screen_clear();
      if (writeEnabled)
      {
        screen_print(0xfb4d3d, " rec ", 0, 0);
      }
      switch (state)
      {
        case Sound:
          screen_print(0x03cea4, " sound ", 0, 10);
          break;
        case Pattern:
          screen_print(0x03cea4, " pattern ", 0, 10);
          break;
      }
      screen_update();
    }
    this->ioCount ++;
  }

  long enc1c = Enc1.read();
  if (enc1c != this->enc1last) {
    this->enc1d = enc1c - this->enc1last;
    this->enc1last = enc1c;
  }
  else
  {
    this->enc1d = 0;
  }
  long enc2c = Enc2.read();
  if (enc2c != this->enc2last) {
    this->enc2d = enc2c - this->enc2last;
    this->enc2last = enc2c;
  }
  else
  {
    this->enc2d = 0;
  }
  // handle updating the value from the encoders
  start[lastPressed] = max(0.0f, min(1.0f, 0.001f*((float)enc1d)+start[lastPressed]));
  length[lastPressed] = max(0.0f, min(1.0f, 0.001f*((float)enc2d)+length[lastPressed]));

}
