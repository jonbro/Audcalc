#include "Sampler.h"

/*******
 * ARRRGH STUPID HACK, JUST COPY PASTED THE WHOLE SCREEN FILE HERE
 */
#include "Screen.h"

Sampler::Sampler()
  : Enc1(4,3), Enc2(6,5),
  step(0), next(0)
{
  this->m.setup();
  this->ioCount = 0;
  this->lastIOUpdate = -1;
  ScreenSetup();
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
  m.setLed((step - 1 + 16) % 4, ((step - 1 + 16) / 4) % 4 + 1, 0);
  m.setLed(step % 4, (step / 4) % 4 + 1, 4);
  next = millis() + 125;
  step++;  
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
    }
    else
    {
      
      //screen.clear();
      //screen.print(0xff00ff, "debug output", 0,0);
      ScreenPrint(0x0000ff, "debug output", 0,0);
      ScreenPrint(0xffff00, "REC: 0", 0,10);
      for (int x = 0; x < 5; x++)
      {
        for (int y = 0; y < 5; y++)
        {
          if(pressed[x][y])
          {
            //screen.print(0xffff00, "pressed", 0, 8);
          }
        }
      }
      ScreenLoop();
    }
    this->ioCount ++;
  }

  long enc1c = Enc1.read();
  if (enc1c != this->enc1last) {
    this->enc1d = enc1c - this->enc1last;
    this->enc1last = enc1c;
  }
  else
  {this->enc1d = 0;}
  long enc2c = Enc2.read();
  if (enc2c != this->enc2last) {
    this->enc2d = enc2c - this->enc2last;
    this->enc2last = enc2c;
  }
  else
  {this->enc2d = 0;}
}
